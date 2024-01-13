/**************************************************************
	Headers
**************************************************************/
// #include "mainHeaders.hpp"
#include "Headers.hpp"


#include "./Facilities/InterThreadQueueSDR7320.hpp"

#include "HW.hpp"
#include "SYNC_BS.hpp"
#include "PSS.hpp"
#include "SSS.hpp"
#include "SF_IN.hpp"
#include "SF_OUT.hpp"
#include "RANDOM_DATA_FOR_PERFORMANCE_EST.hpp"

/*
#include "Headers.hpp"
*/


class PHY_BS
{
  public:
  PHY_BS(){};
  ~PHY_BS(){};


  void makeBSthread(InterThreadQueueSDR7320<TXitem> &txQueue,InterThreadQueueSDR7320<RXitem> &rxQueue)
  {
    PHY_THREAD_KEEP_GOING.store(true);

    m_thread = std::thread(std::bind(&PHY_BS::runBS,this,std::ref(txQueue),std::ref(rxQueue)));
    std::cout << "the BS thread started, joinable: " << m_thread.joinable() << '\n';
    //m_thread.join();
  };


  public:

  int runBS(InterThreadQueueSDR7320<TXitem> &txQueue, InterThreadQueueSDR7320<RXitem> &rxQueue)
  {
    int retVal = EXIT_SUCCESS;
        
    //Find USRP address:
    uhd::device_addr_t args;
    //Try to find address from connected USRP and store it in args. Check if address is found
    if (uhd::device::find(args).size() == 0) std::cout<<"\n No device found!";

    // init USRP
    /**********************************************************************/
    // config information
    int usrp_type = 0; // 0 - usb; 1 - ethernet
    int scaleSpectrum = 4;
//    double rate = (1.92e6*scaleSpectrum);
    double rate = (8000000.0);

    //double dl_freq(2.4e9);
    double dl_freq = 485e6;
//    double dl_freq = 100e6;
  
    //double ul_freq(2.42e9);
    double ul_freq(495e6);
    //double ul_freq(105e6);
    //  double dl_rate(2e6);
    //  double ul_rate(2e6);
    double dl_rate(rate);
    double ul_rate(rate);
//    double dl_rate(1.92e6*scaleSpectrum);
//    double ul_rate(1.92e6*scaleSpectrum);
    double tx_gain(65);;
//    double rx_gain(45);
//     double rx_gain(35);
    double rx_gain(50);
    int fft_size = 128*scaleSpectrum;
    int fft_size_search = fft_size*8;
    int NRBDL = 6*scaleSpectrum; 
    int nr_symbols = 6;
    int phy_packet_size = NRBDL*12/2*nr_symbols;
  
    int cell_id = 0;

    std::vector<std::complex<float> > tx_buff(0);
    std::vector<std::complex<float> > buff(0);
    std::vector<std::complex<float> *> buffs(1, &buff.front());
    std::vector<int> tx_data(0);

    //uhd::device_addr_t args = (uhd::device_addr_t)"serial=308F980";

  /**********************************************************************/
  // instances   
  // HW      - USRP transmitter/receiver
  // SYNC_BS - subframe sync and counters 
  // SF_OUT   - subframe structure for transmission 
  // SF_IN   - subframe structure for reception, equalization and ber computation
  HW hw(usrp_type, args, dl_freq, dl_rate, ul_freq, ul_rate,  tx_gain, rx_gain);
//  SYNC_BS sync_bs(cell_id,fft_size,hw);
  auto sync_bs = new SYNC_BS(cell_id,fft_size,hw);

  SF_OUT* sf_bs = new SF_OUT(fft_size,NRBDL,cell_id);
  SF_IN* sf_in = new SF_IN(fft_size,NRBDL,cell_id+1);

//  SF_OUT sf_bs(fft_size,NRBDL,cell_id);
//  SF_IN sf_in(fft_size,NRBDL,cell_id+1);
    

  /**********************************************************************/
  // transmission
  sync_bs->startUSRP();

//  RANDOM_DATA_FOR_PERFORMANCE_EST testData(fft_size,NRBDL);
  RANDOM_DATA_FOR_PERFORMANCE_EST* testData = new RANDOM_DATA_FOR_PERFORMANCE_EST(fft_size,NRBDL);

  std::vector<int> tmp_tx;
//  std::vector<std::complex<float>> tmp_rx(1024); 

  float accumPower=0;

    // TxRx 
    while(true)
      {

        if (!PHY_THREAD_KEEP_GOING.load())
          break;
         
        
           
   //inx++;
   //std::cout<<" now in the loop:"<<inx<<std::endl;
/*
   sync_bs.getSF();
   if(sync_bs.isTransmissionTime()==true)
    {
     int retVal = sf_bs.prepareTx(tx_buff,testData.m_data);
     if(retVal==0)
     {
       size_t samples_to_send = tx_buff.size();
       sync_bs.sendTX(tx_buff,(size_t)tx_buff.size());
       //std::cout<<sync_bs.getRxTicks()<<" "<<sync_bs.getFrameNumber()<<" "<<sync_bs.getSlotNumber()<<" "<<sync_bs.getSlotCount()<<std::endl;
      }
     }
*/        
         
       sync_bs->getSF();
       
       /**************************************************/
       // TX part 
       if(sync_bs->isTransmissionTime()==true)
       {
         // send txItem
        
        
        auto txItem = std::move(txQueue.readItem());
      	 if(txItem != nullptr)
          {
           if((txItem.get()->elem.size()) == phy_packet_size)
           {
            // std::cout<<txItem.get()->tx_meta_data.num_tx_symbols <<" " << txItem.get()->elem.size() <<std::endl;
            for(int i1=0; i1<phy_packet_size; i1++)
              {tmp_tx.push_back((int)txItem.get()->elem.at(i1));}
            //std::cout<<(int)txItem.get()->elem.at(i1)<<" ";}
            //std::cout<<std::endl;

            int retVal = sf_bs->prepareTx(tx_buff,tmp_tx);
            tmp_tx.clear();
           }
           else
           {
            tmp_tx.clear();
            int retVal = sf_bs->prepareTx(tx_buff,tmp_tx);
            //int retVal = sf_bs->prepareTx(tx_buff,testData->m_data);
           }
            
            if(retVal==0)
            {
              size_t samples_to_send = tx_buff.size();
              sync_bs->sendTX(tx_buff,(size_t)tx_buff.size());
            //std::cout<<sync_bs.getRxTicks()<<" "<<sync_bs.getFrameNumber()<<" "<<sync_bs.getSlotNumber()<<" "<<sync_bs.getSlotCount()<<std::endl;
            }
            
            //}
          }          
         else
          {

          //std::cout<<"no tx data"<<std::endl;
          // send filling packet
          
          // auto tmpTxBuff = txSubframeCreator->makeTXSubFrame();
          
          tmp_tx.clear();
          int retVal = sf_bs->prepareTx(tx_buff,tmp_tx);
          // int retVal = sf_bs->prepareTx(tx_buff,testData->m_data);
           if(retVal==0)
           {
//             size_t samples_to_send = tx_buff.size();
             sync_bs->sendTX(tx_buff,(size_t)tx_buff.size());
           //std::cout<<sync_bs.getRxTicks()<<" "<<sync_bs.getFrameNumber()<<" "<<sync_bs.getSlotNumber()<<" "<<sync_bs.getSlotCount()<<std::endl;
           } 

          }

        txItem.reset();
       }

        // process Rx frame 
       if(sync_bs->isReceptionTime()==true)
       {
        //std::cout<<sync_bs.m_slot_count<< std::endl;
     
        // commented for testing
        int rxState = sf_in->processRx(sync_bs->getSFStart());
//        std::cout<<rxState<<std::endl;
        {
          auto rxItem = std::unique_ptr<RXitem>(new RXitem());
          for(int i1=0;i1< sf_in->getDataLength();i1++) // phy_packet_size
          {
//            rxItem->elem.push_back((int)((sf_in->getBPSKSoftBits()[i1]<0))); //rxData->at(i1).real());
            rxItem->elem.push_back((float)((sf_in->getBPSKSoftBits()[i1]))); //rxData->at(i1).real());
            rxItem->rx_meta_data.elem_soft_bits.push_back(sf_in->getEqualizedSymbols()[i1]); //rxData->at(i1).real());
          }
          rxItem->rx_meta_data.pilot_power = sf_in->getPilotPower();
          rxItem->rx_meta_data.data_power = sf_in->getDataPower();

          rxQueue.writeItem(std::move(rxItem));
/*
          if(10*std::log10(std::abs((sf_in->getPilotPower()/sf_in->getDataPower()))) < 3 && 10*std::log10(std::abs(sf_in->getPilotPower()/accumPower))<3)
          {
            testData->processRxData(sf_in->getBPSKSoftBits(),sf_in->getEqualizedSymbols(),sf_in->getDataLength());
          }
*/
          accumPower = 0.6*sf_in->getPilotPower()+(1-0.6)*accumPower;

        }
       }
     }

    delete testData;
    delete sf_bs;
    delete sync_bs;
    return retVal;
  };
  
  
  public:

  inline void stopProcessing()
  {
     PHY_THREAD_KEEP_GOING.store(false);
     m_thread.join();
  };
  
  public:
  std::atomic_bool PHY_THREAD_KEEP_GOING;
  std::thread m_thread;
};






