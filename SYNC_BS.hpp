/*************************************************************************
*
* Matches IQ samples to the frame structure in BS 
* keeps track of frame and subframe numbers
* 
* Scheduls transmission and reception according to frame numbers
* 
**************************************************************************/

#pragma once 

#include "Headers.hpp"
#include "HW.hpp"
#include "PSS.hpp"
#include "SSS.hpp"

#include <iostream>
#include <fstream>
using namespace std;

class SYNC_BS
{
public:
	public:
	// w_cell_id          - cell id as in LTE. Selects primary and secondary synchronization sequence. 
	// w_fft_size         - fft_size (max 2048 for 20MHz transmission) if less than max spectrum is scaled down
	// w_hw               - instance handling samples transmission to/from USRP
	//
	SYNC_BS(int w_cell_id, int w_fft_size, HW &w_hw):cell_id(w_cell_id), m_fft_size(w_fft_size),m_hw(w_hw)
	{
	
	 // N_id_1 - pss selection
	 // N_id_2 - sss selection
	 N_id_1 = cell_id%3;
	 N_id_2 = (cell_id-N_id_1)/3;
	
	 int m_frame_number = 0;
	 int m_slot_number = 0;

	 m_spectrum_scaling = 2048/m_fft_size;	 
	 m_subframe_size = m_subframe_size/m_spectrum_scaling;
	 m_slot_size = m_subframe_size/2;
	 
	 m_tx_period = m_subframe_size*10; // one tx in every 10 samples
	 m_tx_advance = m_slot_size*2; // schedules transmission two slots ahead 
	 	 
	 for(int i1=0;i1<7;i1++)
         {
  	   cp[i1] = cp_max[i1]/m_spectrum_scaling;
         }	
         
         // shapes the symbol rize by Gaussian function. Takes all CP length of the rize
         maskLong = new float[cp[0]]; // longer cyclic prefix
         for(int i1=0;i1<cp[0];i1++)
         {
  	   maskLong[i1] = exp(-(i1-cp[0]-1)*(i1-cp[0]-1)/((cp[0]-1)*(cp[0]-1)/2/2));
         }
         maskShort = new float[cp[1]]; // shorter cyclic prefix
         for(int i1=0;i1<cp[1];i1++)
         {
  	   maskShort[i1] = exp(-(i1-cp[1]-1)*(i1-cp[1]-1)/((cp[1]-1)*(cp[1]-1)/2/2));
         }

	 m_rx.resize(m_subframe_size);

	 m_pss = new PSS(m_fft_size,m_search_fft_size);

        // subframe structure 	 
	 m_tmp_subframeT = new cFloat*[7];
	 m_tmp_subframeF = new cFloat*[7];
	  for(int i1 =0;i1<7;i1++)
	  {
		m_tmp_subframeF[i1]= new cFloat[1024];
		m_tmp_subframeT[i1]= new cFloat[1024];
	  }
	 m_fft_SF_T2F=(fftwf_plan*) malloc(sizeof(fftwf_plan*)*7);
	  for(int i1 =0;i1<7;i1++)
	  {
      		m_fft_SF_T2F[i1] = fftwf_plan_dft_1d(m_fft_size,(fftwf_complex*)&m_tmp_subframeT[i1][0],(fftwf_complex*)&m_tmp_subframeF[i1][0],FFTW_FORWARD,FFTW_ESTIMATE);
	  }

	};

	~SYNC_BS()
	{
		delete(m_pss);
		for(int i1=0;i1<7;i1++)
		{
			delete(m_tmp_subframeT[i1]);
			delete(m_tmp_subframeF[i1]);
			fftwf_destroy_plan(m_fft_SF_T2F[i1]);
		}
		delete(m_tmp_subframeT);
		delete(m_tmp_subframeF);
		free(m_fft_SF_T2F);
		
		delete(maskLong);
		delete(maskShort);
	};

	// Stsarts USRP and initializes counters 
	//
	int startUSRP()
	{
	  
	  m_hw.startUSRP();
	
	  m_slot_count = -1;
	  m_slot_number = -1;
	  m_frame_number = -1;
	  
	  m_next_slot_to_tx = m_tx_slots_schedule_ahead;
	  m_samples_next_tx = m_hw.getRxTicks() + m_slot_size*(m_tx_slots_schedule_ahead-1);
	  m_samples_next_rx = m_hw.getRxTicks();
	  
	  std::cout<<"startUp The estimated Ticks count:"<<m_samples_next_rx<<" actual count:"<<m_hw.getRxTicks()<<std::endl;
	  
	  return 0;
	};
	
	// just forwards the tx subframe for transmission from radio frontend 
	//
	int sendTX(std::vector<cFloat> &w_data, size_t samples_to_send)
	{
	 
	  int retVal = m_hw.transmitSamples(w_data,samples_to_send,m_samples_next_tx);
	  // TODO: handle execptions
	  	  
	  return 0;
	};
	
	// receives the sample and adjusts frame counts if some samples from IQ frontend are lost
	//
        int getSF()
	{

          // m_samples_next_rx keeps track of the subframe boarders in terms of received samples/Ticks
	  if(m_hw.getRxTicks() != m_samples_next_rx)
	  {
	    
	    using namespace std::chrono;
	    uint64_t t1 =  std::chrono::duration_cast<milliseconds> (std::chrono::system_clock::now().time_since_epoch()).count();
	    // std::cout<<"time [ms]:"<<t1<<" "<<(t1 - tmpTime)<<" expected Ticks count: "<<m_samples_next_rx<<" actual: "<<m_hw.getRxTicks()<<std::endl;
	    
	    tmpTime = t1; 
	    
	    
	    int delta_rx_and_counter = m_hw.getRxTicks() - m_samples_next_rx;
	
	    if(abs(delta_rx_and_counter)<=9)
	    // adjust if there is less than 3 samples drift
	    {
	      m_samples_next_rx = m_hw.getRxTicks();
	      m_samples_next_tx += delta_rx_and_counter;
	    }
	    else if(delta_rx_and_counter > 9)
	    //burn some subframes
	    { 
	      
	      while((delta_rx_and_counter - m_slot_size) > 0)
	      {
	         m_samples_next_rx += m_slot_size;
	         m_samples_next_tx += m_slot_size;
	         m_slot_number = this->updateSlotNumberByOneStep(); 
	         m_frame_number = this->updateFrameNumberByOneStep(); 
	         delta_rx_and_counter -= m_slot_size;
	      }
	      
	      // one subframe if delta is less than subframe
	      if(delta_rx_and_counter < m_slot_size)
	      {
	        m_samples_next_rx += (m_slot_size-delta_rx_and_counter);
	        m_hw.burnSamples(m_slot_size-delta_rx_and_counter);
	        
	        // adjust frame counters
	        m_slot_number = this->updateSlotNumberByOneStep(); 
	        m_frame_number = this->updateFrameNumberByOneStep(); 
	        
                //m_samples_next_rx = m_samples_next_rx + m_slot_size;
                m_samples_next_rx = m_hw.getRxTicks();
                m_samples_next_tx = m_samples_next_tx + (m_slot_size-delta_rx_and_counter);
	      }
	    }
	    else //(deltaRxandCounter < -3)
	    //catch up the subframe border
	    { 
	      m_hw.burnSamples(abs(delta_rx_and_counter));   
	      m_samples_next_rx = m_hw.getRxTicks(); 
	      m_samples_next_tx = m_hw.getRxTicks() + m_slot_size*(m_tx_slots_schedule_ahead-1);
	      // TODO: check that frame numbers are correct
	    }
	    
	    }
	    	     
	  // std::cout<<" expected samples:"<<m_samples_next_rx<<" actual:"<<m_hw.getRxTicks()<<std::endl;
	  if(m_hw.getRxTicks() == m_samples_next_rx)
	  {

          // reads samples in symbol by symbol	  
          int cp_len;
          int nr_of_samples;
          for(int i1=0;i1<7;i1++)
          {
		cp_len=cp[i1];
		nr_of_samples = cp_len + m_fft_size;
          	m_hw.getSamples(m_rx,nr_of_samples);
		std::copy(&m_rx[cp_len],&m_rx[cp_len]+m_fft_size,&m_tmp_subframeT[i1][0]);
		fftwf_execute(m_fft_SF_T2F[i1]);           
          }
          
	  // Update SF counters  
	  m_slot_number = this->updateSlotNumberByOneStep(); 
	  m_frame_number = this->updateFrameNumberByOneStep(); 
	  m_samples_next_rx = m_samples_next_rx + m_slot_size;
	  m_samples_next_tx = m_samples_next_tx + m_slot_size;

	  // if(m_samples_next_rx!=m_hw.getRxTicks())
	  //	std::cout<<m_samples_next_rx<<" "<<m_hw.getRxTicks()<<std::endl;
	  
	  // TODO: make corrections if samples are lost 
           if(m_hw.getRxTicks() != m_samples_next_rx)
           {
             if(m_hw.getRxTicks() > m_samples_next_rx)
             {
               int tmp_diff_in_sf_samples = m_hw.getRxTicks() - m_samples_next_rx;
               float nr_slot = ((float)tmp_diff_in_sf_samples)/((float)m_slot_size);
               int samples_to_burn = ceil(nr_slot)*m_slot_size - tmp_diff_in_sf_samples;
               m_hw.burnSamples(samples_to_burn);
               m_samples_next_rx +=ceil(nr_slot)*m_slot_size;
               m_samples_next_tx +=(ceil(nr_slot)+m_tx_slots_schedule_ahead-1)*m_slot_size;
               
               /*
               if(m_hw.getRxTicks() != m_samples_next_rx)
               {
               
               	std::cout<<" actual clock and estimated clocs are different. actual:"<<m_hw.getRxTicks()<<" estimated:"<<m_samples_next_rx<<std::endl;
               }
               */
               m_slot_count +=(nr_slot+1);
             }
           }
         
	    m_slot_count = m_slot_count%20;
	    m_next_slot_to_tx = (m_slot_count + m_tx_slots_schedule_ahead)%20;
           
          // HACK: tests what is the Ticks difference between the BS and UE 
	  if(m_slot_count==10) // correlate with the adjustment code. 
	  {
	  // correlation
	  // peak search 
	  // delta peak difference 
           m_pss->cellTracking(&m_rx[0],N_id_1+1);
          
           double max_value = m_pss->getMaxCorrValue(N_id_1+1);
	   int max_location = m_pss->getMaxCorrValueLocation(N_id_1+1);
	    
	   int m_tracking_adjustment = max_location - cp[6];
	   //std::cout<<"Tracking max_v: "<<max_value<<" max_loc: "<<max_location<<" adjustment"<< m_tracking_adjustment;
	   //std::cout<<" frame nr:"<<m_frame_number<<" slot nr:"<<m_slot_number<<" slot count"<<m_slot_count<<std::endl;
	  }
	    
	    //std::cout<<" after reception:"<<m_samples_next_rx<<std::endl;
	  }
          // std::cout<<"frame nr:"<<m_frame_number<<" slot nr:"<<m_slot_number<<" slot count:"<<m_slot_count<<std::endl;
          
	  return 0;
	};
	
	//
	int updateSlotNumberByOneStep()
	{
	  m_slot_number++;
	  m_slot_number = (m_slot_number%2);
	  m_slot_count++;
	  return m_slot_number;
	};
	
	//
	int updateFrameNumberByOneStep()
	{
	  if(m_slot_number == 0)
	    {		  
	      m_frame_number++;
	      m_frame_number = (m_frame_number%10);
	    } 
	  return m_frame_number;
	};
	
	/////////////////////////////////////////////////////////////////////
	// various internal parameters 
	//	
	bool isTransmissionTime()
	{
		if(m_slot_count == m_tx_slot)
		 return true;
		
		return false;
	};
	
	bool isReceptionTime()
	{
	  if(m_slot_count==10)
	    return true;
	    
	  return false;	
	};

	
	int getNRBDL()
	{
		return NRBDL;
	};
	int getCellID()
	{
		return cell_id;
	};

	int getN_id_1()
	{
		return N_id_1;
	};
	int getN_id_2()
	{
		return N_id_2;
	};
	int getFrameNumber()
	{
		return m_frame_number;
	};
	int getSlotNumber()
	{
		return m_slot_number;
	};
	
	int getSlotCount()
	{
          return m_slot_count;
        };
	
	// returns pointer to the fft of the sybmol w_symbol_nr
	cFloat** getSFStart()
	{
		return m_tmp_subframeF;
	};
	
	unsigned long long getRxTicks()
	{
		return m_hw.getRxTicks();
	};
	
public:
	HW m_hw;
	int m_fft_size = 2048;
	int m_search_fft_size = 2048*8;
	
        std::vector<std::complex<float>> m_rx;

	uint64_t tmpTime = 0;
	PSS* m_pss;
	SSS m_sss;
	int NRBDL = 50;
	
	int N_id_1 = -1;
	int N_id_2 = -1;
	int cell_id = -1;
	cFloat **m_tmp_subframeT;
	cFloat **m_tmp_subframeF;
	int m_frame_number=-1;
	int m_slot_number=-1;
	int m_slot_count = -1;
	int m_next_slot_to_tx = -1;
	int m_tx_slots_schedule_ahead = 8; // the subframe number is looked when it has arrived already
	int m_tx_slot = 20 - m_tx_slots_schedule_ahead; // starts to transmit three subframes ahead.  
	unsigned long long m_sample_tracking = 0;

	unsigned long long m_tx_period = 0;
	unsigned long long m_tx_advance = 0;
	unsigned long long m_samples_next_tx = 0;
	unsigned long long m_samples_next_rx = 0;


	// internal trackers
	int m_subframe_size = 30720;
	int m_slot_size = 30720/2;
	int m_spectrum_scaling = 1;
 
        int cp_max[7] = {160,144,144,144,144,144,144}; 	
        int cp[7];
        float *maskLong;
        float *maskShort;
	
	fftwf_plan *m_fft_SF_T2F;
   
};


