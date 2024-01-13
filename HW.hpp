/***************************************************************************
*
* reading and writing to USRP 
*
****************************************************************************/

#pragma once

#include "Headers.hpp"

/*
* inits the HW and reads writes IQ samples to the frontend 
* 
*
*/ 

class HW
{
public: 
  // w_usrp_type - 0 or 1. 
  //       0 - B210 usb board
  //       1 - N200 ethernet connected
  // w_args     - device id or ethernet address 
  // w_tx_freq  - tx frequency 
  // w_tx_rate  - tx sampling rate
  // w_rx_freq  - rx frequency
  // w_rx_rate  - rx sampling rate 
  // w_tx_gain  - tx amplifier gain (notice: different scale for different USRPs)
  // w_rx_gain  - rx amplifier gain
  HW(int w_usrp_type, uhd::device_addr_t w_args, double w_tx_freq, double w_tx_rate,
  double w_rx_freq, double w_rx_rate,  double w_tx_gain, double w_rx_gain):
  m_usrp_type(w_usrp_type),m_args(w_args), m_tx_freq(w_tx_freq),m_tx_rate(w_tx_rate),
  m_rx_freq(w_rx_freq),m_rx_rate(w_rx_rate),m_tx_gain(w_tx_gain),m_rx_gain(w_rx_gain)
  {

  // buffer for reading data from the radio interface 
  m_tmp_buff.resize(m_subframe_size);
  m_tmp_buffs.push_back(&m_tmp_buff.front());	
  
  // creation of the interface 
  m_txrx_chain_delay=26; // default for Ethernet device
  if(m_usrp_type == 0)
  {
  std::cout<<"USRP type USB \n"<<std::endl;
  m_txrx_chain_delay = 95; //66 // that is correct only for certain sampling rate 
  //m_tx_gain = 65;
  //m_rx_gain = 95;
  //args =  (uhd::device_addr_t)"serial=308F980"; // 30FB577
  //uhd::device_addr_t args("serial=3131082");
  }
  else
  {
  std::cout<<"USRP type Ethernet \n"<<std::endl;
  m_txrx_chain_delay = 26; //25; 
  // m_tx_gain = 5;
  // m_rx_gain = 25;
  // args = ( uhd::device_addr_t) "addr=192.168.10.2"; // fore Ethernet unit we need addr
  }
  
  uhd::device_addrs_t dev_addrs = uhd::device::find(m_args);
  if (dev_addrs.size()==0) std::cout<<"\n No device found!";
  // create device 
  m_usrp = uhd::usrp::multi_usrp::make(dev_addrs[0]);
  
  
  m_usrp->set_tx_antenna("TX/RX");
  m_usrp->set_tx_gain(m_tx_gain);
  m_usrp->set_tx_freq((double)m_tx_freq);
  m_usrp->set_tx_rate(m_tx_rate);
  
  uhd::stream_args_t stream_args("fc32", "sc16");
  stream_args.channels = { 0 };
  m_tx_stream = m_usrp->get_tx_stream(stream_args);
  
  m_usrp->set_rx_antenna("RX2");
  m_usrp->set_rx_gain(m_rx_gain);
  m_usrp->set_rx_freq((double)m_rx_freq);
  m_usrp->set_rx_rate(m_rx_rate);
  m_usrp->set_rx_agc(false, 0);// disable agc
  m_rx_stream = m_usrp->get_rx_stream(stream_args);
  
  // Lock mboard clocks  
  std::string ref("internal");
  // std::cout << boost::format("Lock mboard clocks: %f") % ref << std::endl;
  m_usrp->set_clock_source(ref);
  
  //std::cout<<m_txrx_chain_delay<<std::endl;
  std::cout<<"UHD instance created"<<std::endl;    
  };
  
  ~HW()
  {
  };


  // first intializes the USRP into streaming mode
  // reads out the samples from the RX buffer
  //   the last readout start gives the sample number 
  //   in USRP. All samples numbered by (unsigned long long)
  //
  int startUSRP()
  {
    
    // reset into streaming mode
    m_cmd = uhd::stream_cmd_t::STREAM_MODE_STOP_CONTINUOUS;
    m_usrp->issue_stream_cmd(m_cmd);
    m_rx_stream->issue_stream_cmd(m_cmd);

    float delay =0.5;
    m_cmd.num_samps = m_tmp_buff.size();           // hwo many samples will be read
    m_cmd.stream_now = false;                      // no start yet
    m_cmd.time_spec  = uhd::time_spec_t(delay);    // Arbitrary delay before to start streaming. gives time for USRP to prepare. 
    m_usrp->set_time_now(uhd::time_spec_t(0.0));   // 

    m_cmd = uhd::stream_cmd_t::STREAM_MODE_START_CONTINUOUS;
    m_usrp->issue_stream_cmd(m_cmd);	
    m_rx_stream->issue_stream_cmd(m_cmd);

    // reads the rx buffer till it is empty
    m_rx_Ticks = 0;
    int iter_inx = 0;
    while(true)
    {
      m_rx_stream->recv(m_tmp_buffs, 1, m_rx_md, m_cmd.time_spec.get_full_secs() + 2.1);
      printf("buffer clearing iteration %i \n",iter_inx);

      if(m_rx_md.error_code == uhd::rx_metadata_t::ERROR_CODE_NONE)
      {
        m_rx_Ticks = m_rx_md.time_spec.to_ticks(m_rx_rate) + 1;
        break;
      }
      iter_inx++;
    }
  
    std::cout<<std::endl<<"rx_Ticks:"<<m_rx_Ticks<<std::endl;	
    std::cout<<"USRP started:"<<std::endl;
    return 0;
  };
  
  // Reads out samples from USRP without doing anything to them
  // used for getting first RX sample to have certain Ticks (count number)
  // w_samples_to_burn - nr of samples read out	
  //
  int burnSamples(int w_samples_to_burn)
  {
    int samples_to_burn = w_samples_to_burn;
	  
	  
    // expected Ticks after burning samples 	  
    m_rx_Ticks_expected = m_rx_Ticks + w_samples_to_burn;
    //std::cout<<"w_samples_to_burn:"<<w_samples_to_burn<<" m_rx_Ticks:"<<m_rx_Ticks<<" expected ticks:"<<m_rx_Ticks_expected<<" buffer size:"<<m_tmp_buff.size()<<std::endl;
    
    // readout buffer size
    if(m_tmp_buff.size() == 0)
      m_tmp_buff.resize(m_subframe_size/2);
	  	      
	  	      
    // iterates till the number of read out samples is the same as w_samples_to_burn 
    //    return 0
    // If the cout get messed up (like lost packet 
    //    return 1
    unsigned long long init_rx_Ticks = m_rx_Ticks;
    int tmpSamps = 0;
    int tmp_buffer_size = m_tmp_buff.size();
    while((w_samples_to_burn-tmpSamps) > 0)
      {
        if(tmp_buffer_size > (w_samples_to_burn-tmpSamps))
	  {
	    tmp_buffer_size = w_samples_to_burn-tmpSamps;
	  }
	auto num_rx_samps = m_rx_stream->recv(m_tmp_buffs, tmp_buffer_size, m_rx_md, m_cmd.time_spec.get_full_secs() + 0.0001);		  
	    	
	 if(num_rx_samps>0)
	  {
	    m_rx_Ticks = (m_rx_md.time_spec.to_ticks(m_rx_rate)+num_rx_samps);
	    tmpSamps = m_rx_Ticks - init_rx_Ticks;
	  }
	  //std::cout<<" Ticks:"<<m_rx_Ticks<<"  tmp-samps:"<<tmpSamps<<" received samps:"<<num_rx_samps<<std::endl;  	
      }
      // std::cout<<" m_rx_Ticks"<<m_rx_Ticks<<" expected ticks:"<<m_rx_Ticks_expected<<std::endl;
    
    // something went wrong
    if(m_rx_Ticks_expected != m_rx_Ticks)
      return 1;
    
    // we are good
    return 0;
   };
    
    
    // gets buffer full of IQ samples 
    // o_buff     - sample buffer 
    // number_of_samples_to_be_received
    
    
    /*
    Return state
     - No error 
         return 0
     - samples_lost_in_the_middle
         return 1
     - samples_lost_over_packet_border !!NOT HANDLING FOR NOW
         return 2
    */
    int getSamples(std::vector<cFloat> &o_buff,int number_of_samples_to_be_received)
    {
	//std::cout<<&o_buff<<" buffer to get size:"<<number_of_samples_to_be_received<<std::endl;

	// the ticks count after successful reception of samples
	m_rx_Ticks_expected = m_rx_Ticks + number_of_samples_to_be_received;

        // buffer will fit all the samples
	if(o_buff.size()<number_of_samples_to_be_received)
          {
            o_buff.resize(number_of_samples_to_be_received);
          }		
	std::fill(&o_buff[0],&o_buff[0]+number_of_samples_to_be_received, (cFloat)(0.0, 0.0));

        
	auto samples_to_be_received = number_of_samples_to_be_received;
	if(m_tmp_buff.size()!=samples_to_be_received)
		m_tmp_buff.resize(samples_to_be_received);
		
       // reads number_of_samples_to_be_received IQ samples from  from USRP frontend into o_buffer 	
	unsigned long long init_rx_Ticks = m_rx_Ticks;
	int tmpSamps = 0;
	int expected_location_in_the_frame =0;
	bool some_samples_lost = false;
	while(tmpSamps < samples_to_be_received)
	  {
	  
	    auto num_rx_samps = m_rx_stream->recv(m_tmp_buffs, samples_to_be_received - tmpSamps, m_rx_md, m_cmd.time_spec.get_full_secs() + 0.0001);
	    long long tmp_rx_Ticks = m_rx_md.time_spec.to_ticks(m_tx_rate);
		 	  
	    if(num_rx_samps>0)
	     {
	        // ideally the Ticks count should align with the location in the frame
		int location_in_the_frame = tmp_rx_Ticks-init_rx_Ticks;		
	        // it seems sometimes the counter in USRP is reset -> we use tmpSamps then
		if(location_in_the_frame < 0)
		  location_in_the_frame = tmpSamps;

		expected_location_in_the_frame += num_rx_samps;
		if(expected_location_in_the_frame!=location_in_the_frame)
		  some_samples_lost = true;
		// expected_location_in_the_frame = location_in_the_frame; // reset to be current location
		
               // if not packet/smaples lost the location/amount of received samples is always less that required amount		    
		if(location_in_the_frame < samples_to_be_received)
		{
		  // the samples count does not go over the packet border 
		  if( (location_in_the_frame+num_rx_samps)<= samples_to_be_received)
		   {
  		    //std::copy(&m_tmp_buffs[0][0],&m_tmp_buffs[0][num_rx_samps],&o_buff[tmpSamps]);
  		    std::copy(&m_tmp_buffs[0][0],&m_tmp_buffs[0][num_rx_samps],&o_buff[location_in_the_frame]);
  		   }
  		   else
  		   {
  		     int num_rx_samples_copied = samples_to_be_received - location_in_the_frame;
  		     std::copy(&m_tmp_buffs[0][0],&m_tmp_buffs[0][num_rx_samples_copied],&o_buff[location_in_the_frame]);  		      
  		   }
		}	

		// new timing moment
		m_rx_Ticks = tmp_rx_Ticks + num_rx_samps;
		unsigned long long rx_TicksOld = m_rx_Ticks; 
		
	    	tmpSamps +=num_rx_samps; 
	      }
	   }

	  if(some_samples_lost == true)
	    return 1;
	  if(m_rx_Ticks_expected < m_rx_Ticks)
	    return 2;
	  return 0;	
	};

   // non streaming transmission of the samples out from USRP
   // w_data             - data to be sent
   // w_samples_to_send  - 
   // w_tx_time          - when is sent out  
   // everything OK 
   //   return 0
   // send samples different than w_samples_to_send 
   //   return 1
   int transmitSamples(std::vector<cFloat> &w_data,size_t w_samples_to_send, unsigned long long w_tx_time)
   {
   
     // Non streaming mode
     // packet has start and end 
     // sent out at time_spec moment
     uhd::tx_metadata_t tx_metadata; 
     tx_metadata.start_of_burst = true;
     tx_metadata.end_of_burst = true;
     tx_metadata.has_time_spec  = true;
     //std::cout<<m_txrx_chain_delay<<std::endl;
     tx_metadata.time_spec = uhd::time_spec_t::from_ticks(w_tx_time-m_txrx_chain_delay,m_tx_rate);
     
     size_t packet_samps = m_tx_stream->send(&w_data[0], w_samples_to_send, tx_metadata);

     if(packet_samps != w_samples_to_send)
       return 1;   
     return 0;
   }

   // current Ticks count 
   unsigned long long getRxTicks()
   {
      return m_rx_Ticks;
   }	

public:
  /**********************************************************************/
  size_t m_txrx_chain_delay = 0;

  /**********************************************************************/
  uhd::device_addr_t m_args;
  uhd::device_addrs_t m_dev_addrs;

  uhd::usrp::multi_usrp::sptr m_usrp;
  uhd::tx_streamer::sptr m_tx_stream;
  uhd::rx_streamer::sptr m_rx_stream;

  uhd::tx_metadata_t m_tx_md;
  uhd::stream_cmd_t m_cmd = uhd::stream_cmd_t::STREAM_MODE_STOP_CONTINUOUS;
  uhd::rx_metadata_t m_rx_md;


  //set the transmit sample rate
  int m_usrp_type = 1; // 0 - usb; 1 - ethernet
  //double m_dl_freq = 2.4e9;
  double m_tx_freq = 2.4e9;
  double m_tx_rate = (2.0 *1e6);
  double m_tx_gain = 10;
  double m_rx_freq = 2.42e9;
  double m_rx_rate = (2.0 *1e6);
  double m_rx_gain = 30;

  double m_max_sampling_rate = 30720e3;
  double m_sampling_rate;
  double m_spectrum_downscale = 16;
  uint64_t tmp_buffer_size = 30720;
  uint64_t m_subframe_size = 30720;

  unsigned long long m_tx_Ticks = 0;
  unsigned long long m_tx_Ticks_next = 0;
   
  unsigned long long m_rx_Ticks = 0;
  unsigned long long m_rx_Ticks_expected; 
  

  std::vector<std::complex<float>> m_tmp_buff;
  std::vector<std::complex<float>*> m_tmp_buffs;
  
  };
  
