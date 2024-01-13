/**************************************************************
	Headers
**************************************************************/
// #include "mainHeaders.hpp"
#include "Headers.hpp"

#include "./Facilities/InterThreadQueueSDR7320.hpp"

#include "Config.hpp"
// #include "OUT_INTERFACE.hpp"
#include "HW.hpp"
#include "SYNC_UE.hpp"
#include "PSS.hpp"
#include "SSS.hpp"

#include "LFSR.hpp"
#include "SF_IN.hpp"
// #include "SF_OUT.hpp"
#include "RANDOM_DATA_FOR_PERFORMANCE_EST.hpp"

using namespace std;

class PHY_UE
{
public:

  PHY_UE(){};
  ~PHY_UE(){};

public:
  void makeUEthread(InterThreadQueueSDR7320<TXitem> &txQueue,InterThreadQueueSDR7320<RXitem> &rxQueue)
  {
    PHY_THREAD_KEEP_GOING.store(true);

  //  m_thread = std::thread(&PHY_UE::mainUE,this,std::ref(txQueue),std::ref(rxQueue));
    m_thread = std::thread(std::bind(&PHY_UE::runUE,this,std::ref(txQueue),std::ref(rxQueue)));
    std::cout << "the UE thread starting, joinable: " << m_thread.joinable() 
              << '\n';
  //m_thread.join();
  };

  inline int runUE(InterThreadQueueSDR7320<TXitem> &txQueue,InterThreadQueueSDR7320<RXitem> &rxQueue)
  {
    
      /**********************************************************************/
  // config information
  //set the transmit sample rate
  int usrp_type = 0; // 0 - usb; 1 - ethernet
  //Find USRP address:
  uhd::device_addr_t args;
  //Try to find address from connected USRP and store it in args. Check if address is found
  if (uhd::device::find(args).size() == 0) std::cout<<"\n No device found!";
//  uhd::device_addr_t args = (uhd::device_addr_t)"serial=3104B0C";
//"serial=30FB577"; //"serial=308F980";
//  uhd::device_addr_t args = (uhd::device_addr_t) "serial=30FB577";
  int scaleSpectrum = RATE_SCALING; //4;
//  double rate = (1.92e6*scaleSpectrum);
//  double rate = (7692308.0);
  double rate = (8000000.0);

  //double in_freq(2.4e9);
  double in_freq = DL_FREQ;
//  double in_freq = 485e6;  // 
  //double in_freq = 100e6;    // to use over cable only
//  double tx_gain(1); // 75 for B210 1 for X300
//  double tx_gain(60); // 75 for B210 1 for X300
  double tx_gain(45); // 75 for B210 1 for X300
  //double out_freq(2.42e9); // wlan free freq
  double out_freq(495e6);    // uni has license for this freq
  //double out_freq(105e6);  // to use over cable only

//  double in_rate(1.92e6*scaleSpectrum);
//  double out_rate(1.92e6*scaleSpectrum);
  double in_rate = (RATE); //(1.9231e6*scaleSpectrum);
  double out_rate = (RATE); //(1.9231e6*scaleSpectrum);
//  double rx_gain(20);; // 65 for B210
  double rx_gain(50);; // 65 for B210
  int fft_size = FFT_SIZE; //128*scaleSpectrum;
  int fft_size_search = fft_size*8;
  //int NRBDL = 28/scaleSpectrum; 
  int nrbdl = 6*scaleSpectrum; 
  int nr_symbols = 6;
  int phy_packet_size = nrbdl*12/2*nr_symbols;  

  int cell_id = 0;

  int retValOut;

  std::vector<std::complex<float> > tx_buff(0);
  
  // int port = 8090;
  // OUT_INTERFACE out(port);

  /**********************************************************************/
  // instances  
  HW hw(usrp_type, args, out_freq, out_rate, in_freq, in_rate,  tx_gain, rx_gain);
  //hw.startUSRP();
  auto sync_ue = new SYNC_UE(fft_size,fft_size_search,hw);

  SF_IN* sf_in = new SF_IN(fft_size,nrbdl,cell_id);         // Reception subframe with PSS seq cell_id
  SF_OUT* sf_out = new SF_OUT(fft_size,nrbdl,cell_id+1);    // From UE to BS we insert PSS seq cell_id+1 

  RANDOM_DATA_FOR_PERFORMANCE_EST* testData = new RANDOM_DATA_FOR_PERFORMANCE_EST(fft_size,nrbdl);

/*  SYNC_UE sync_ue(fft_size,fft_size_search,hw);
  SF_IN sf_in(fft_size, nrbdl, cell_id);     // Reception subframe with PSS seq cell_id
  SF_OUT sf_out(fft_size, nrbdl, cell_id+1); // From UE to BS we insert PSS seq cell_id+1 

  RANDOM_DATA_FOR_PERFORMANCE_EST testData(fft_size, nrbdl);
*/


    sync_ue->startUSRP();

    std::vector<int> tmp_tx;

    while(true)
    {
    if (!PHY_THREAD_KEEP_GOING.load())
          break;
    
         // state maching for SYNC
        switch(sync_ue->getSyncState())
        {
          case SYNC_STATE::CELL_SEARCH:
          {
            sync_ue->cellSearch();
            break;
          }
          case SYNC_STATE::CELL_TRACKING:
          {
            sync_ue->getSF();
       
            if(sync_ue->isTransmissionTime()==true)
            {
            // fetch data from textData.m_data instance 
            // insert data into tx_buff - pilots, data sync sequnce 
            // send data data out     

             auto txItem = std::move(txQueue.readItem());
      	     if(txItem != nullptr)
              {
                if((txItem.get()->elem.size()) == phy_packet_size)
                {
           
                 for(int i1=0; i1<phy_packet_size; i1++)
                   {tmp_tx.push_back((int)txItem.get()->elem.at(i1));}
                 //std::cout<<(int)txItem.get()->elem.at(i1)<<" ";}
                 //std::cout<<std::endl;

                 retValOut = sf_out->prepareTx(tx_buff,tmp_tx);
                 tmp_tx.clear();
                 // std::cout<<"data tx"<<std::endl;
                }
                else
                {
                  tmp_tx.clear();
                  retValOut = sf_out->prepareTx(tx_buff,tmp_tx);
//                 retValOut = sf_out->prepareTx(tx_buff,testData->m_data);
                }
            
                if(retValOut==0)
                {
                  size_t samples_to_send = tx_buff.size();
                  sync_ue->sendSF(tx_buff,(size_t)tx_buff.size());
                  //std::cout<<sync_bs.getRxTicks()<<" "<<sync_bs.getFrameNumber()<<" "<<sync_bs.getSlotNumber()<<" "<<sync_bs.getSlotCount()<<std::endl;
                }
            
            //}
              }          
              else
              {

              // std::cout<<"no tx data"<<std::endl;
              // send filling packet
          
              // auto tmpTxBuff = txSubframeCreator->makeTXSubFrame();
                tmp_tx.clear();
                retValOut =sf_out->prepareTx(tx_buff,tmp_tx);

//              retValOut = sf_out->prepareTx(tx_buff,testData->m_data);
              if(retValOut==0)
               {
                size_t samples_to_send = tx_buff.size();
                sync_ue->sendSF(tx_buff,(size_t)tx_buff.size());
                //std::cout<<sync_bs.getRxTicks()<<" "<<sync_bs.getFrameNumber()<<" "<<sync_bs.getSlotNumber()<<" "<<sync_bs.getSlotCount()<<std::endl;
               } 
               }
          txItem.reset();
/*
            auto txItem = std::move(txQueue.readItem());



            int retVal = sf_out->prepareTx( tx_buff, testData->m_data);
            if(retVal == 0)
            {
              size_t samples_to_send = tx_buff.size();
               sync_ue->sendSF(tx_buff,(size_t)tx_buff.size());	
            }
*/
             } // if TX 
       
          // Rx Processing. Rx period has 20 subframes
          if(sync_ue->isReceptionTime()==true)
          {

            // commented for testing
            sf_in->processRx(sync_ue->getSFStart());
            {
              auto rxItem = std::unique_ptr<RXitem>(new RXitem());
              for(int i1=0;i1< sf_in->getDataLength();i1++) // phy_packet_size
              {
//                rxItem->elem.push_back((int)((sf_in->getBPSKSoftBits()[i1]<0))); //rxData->at(i1).real());
                rxItem->elem.push_back((float)((sf_in->getBPSKSoftBits()[i1]))); //rxData->at(i1).real());
                rxItem->rx_meta_data.elem_soft_bits.push_back(sf_in->getEqualizedSymbols()[i1]); //rxData->at(i1).real());
              }
              rxItem->rx_meta_data.pilot_power = sf_in->getPilotPower();
              rxItem->rx_meta_data.data_power = sf_in->getDataPower();

              rxQueue.writeItem(std::move( rxItem));
// testData->processRxData(sf_in->getBPSKSoftBits(),sf_in->getEqualizedSymbols(),sf_in->getDataLength());
             }

            //sf_in->processRx(sync_ue->getSFStart()); 	
            //testData->processRxData(sf_in->getBPSKSoftBits(),sf_in->getEqualizedSymbols(),sf_in->getDataLength());                   
          }

          break;
         } 
         default:
         break;
        } // switch

    }  // while 

//    delete sf_in;
    delete testData;
    delete sf_out;
    delete sync_ue;
    return 0;
  };

  inline void stopProcessing()
  {
     PHY_THREAD_KEEP_GOING.store(false);
     m_thread.join();
  };


public:
  std::atomic_bool PHY_THREAD_KEEP_GOING;
  std::thread m_thread;
};

