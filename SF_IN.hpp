/**********************************************************************
*
* Receiver function paired with SF_OUT class
*
* It the pilots - every second OFDM carrier is pilot
* for RX subframe 
*   it estimates the channel 
*   it equalizes the symbols 
*   decodes BPSK symbols to bits
* 
**********************************************************************/

#pragma once

#include "Headers.hpp"
#include "PSS.hpp"
#include "SSS.hpp"
#include "LFSR.hpp"

#include <iostream>
#include <fstream>

class SF_IN
{
public: 
public:
  SF_IN(int w_fft_size,int w_NRBDL,int w_cell_id): m_fft_size(w_fft_size),NRBDL(w_NRBDL),cell_id(w_cell_id)
  {
  // pss 
  N_id_1 = cell_id%3;          // Selects the primary synchronization sequence (PSS)
  N_id_2 = (cell_id-N_id_1)/3; // Selects the secondary synchronization sequence (SSS) - not in use in this version  

  // 
  m_nr_data_symbols = 6;
  m_data_in_symbol = NRBDL*12/2;
  m_data_packet_size = m_data_in_symbol*m_nr_data_symbols;

  // max fft_size = 2048 
  // for other sizes it is downscaled 
  m_spectrum_scaling = 2048/m_fft_size;
  m_subframe_size = 30720/m_spectrum_scaling/2;
  
  // cyclic prefics for this downscaling factor
  for(int i1=0;i1<7;i1++)
    {
  	cp[i1] = cp_max[i1]/m_spectrum_scaling;
    }	
  
  // RX subframe structure 
  m_tmp_rx_subframeT = new cFloat*[7];
  m_tmp_rx_subframeF = new cFloat*[7];
  for(int i1 =0;i1<7;i1++)
  {
    m_tmp_rx_subframeF[i1]= new cFloat[m_fft_size];
    m_tmp_rx_subframeT[i1]= new cFloat[m_fft_size];
  }

  // FFT - conversion from time to frequency domain
  m_fft_SF_RX_T2F=(fftwf_plan*) malloc(sizeof(fftwf_plan*)*7);
  for(int i1 =0;i1<7;i1++)
  {
    m_fft_SF_RX_T2F[i1] = fftwf_plan_dft_1d(m_fft_size,(fftwf_complex*)&m_tmp_rx_subframeT[i1][0],(fftwf_complex*)&m_tmp_rx_subframeF[i1][0],FFTW_FORWARD,FFTW_ESTIMATE);
  }
  
  m_data_soft = new cFloat[m_data_packet_size];
  m_data_est = new int[m_data_packet_size];


  // Pilots 
  m_pilots = new cFloat[m_fft_size];
  m_pilots_conj = new cFloat[m_fft_size];
  m_ch_estimate = new cFloat[m_fft_size];  
  memset( m_pilots, 0, m_fft_size * sizeof(cFloat));
  
  // pseudo random number generation
  LFSR_for_GoldSequence lfsr; // generator
  m_prbs_pilots = new int[2*m_fft_size]; 
  lfsr.getPRBSbits(1300*8, &m_prbs_pilots[0], 2*m_fft_size);
    
  m_pilots_locations = new int[NRBDL*12/2];
  m_pilots_modulated_values = new cFloat[NRBDL*12/2];

  m_data_locations = new int[NRBDL*12/2];
  
  // map to the data locatins
  float sqrt12= sqrt(1.0/2.0);
  int inx=0;
  for(int i1 =(m_fft_size-NRBDL*12/2); i1<m_fft_size; i1+=2)
    {
      m_pilots_locations[inx] = i1;
      m_data_locations[inx] = i1+1;
      m_pilots_modulated_values[inx] = cFloat(sqrt12*(1-2*m_prbs_pilots[2*inx]),sqrt12*(1-2*m_prbs_pilots[2*inx]));
      inx++;
    }
  for(int i1 = 1; i1 < (NRBDL*12/2); i1+=2)
    {
      m_pilots_locations[inx] = i1;
      m_data_locations[inx] = i1+1;
      m_pilots_modulated_values[inx] = cFloat(sqrt12*(1-2*m_prbs_pilots[2*inx]),sqrt12*(1-2*m_prbs_pilots[2*inx]));
      inx++;
    }

  ///////////////////////////////////////////////////////////////  
  float scale1 = (NRBDL*12)/62;
  inx =0;
  for(int i1 =(m_fft_size-NRBDL*12/2);i1<m_fft_size;i1++)
  	{
  	m_pilots[i1] = cFloat(sqrt12*(1-2*m_prbs_pilots[inx]),sqrt12*(1-2*m_prbs_pilots[inx]));//*scale1;
  //	std::cout<<inx<<":"<<m_pilots[i1]<<" "<<(1-2*prbs[2*inx])<<","<<(1-2*prbs[2*inx+1])<<" "<<prbs[2*inx]<<","<<prbs[2*inx+1]<<" ";
  	inx++;
  	//m_pilots[i1].real(-2.0*(i1%2)+1.0);
  	}
  for(int i1 =1;i1 < (NRBDL*12/2+1);i1++)
  	{
  	m_pilots[i1] = cFloat(sqrt12*(1-2*m_prbs_pilots[inx]),sqrt12*(1-2*m_prbs_pilots[inx]));//*scale1;
  //	std::cout<<inx<<":"<<m_pilots[i1]<<" "<<(1-2*prbs[2*inx])<<","<<(1-2*prbs[2*inx+1])<<" "<<prbs[2*inx]<<","<<prbs[2*inx+1]<<" ";
  	inx++;
  	//m_pilots[i1].real(-2.0*(i1%2)+1.0);
  	}

/*
   int sumErr =0;
   inx =0;
   for(int i1=0;i1<(NRBDL*12); i1+=2)
   { 
     if(m_pilots_modulated_values[inx]!=m_pilots[m_pilots_locations[inx]])
      sumErr++;	
     inx++;
   }
   std::cout<<"Pilot errors:"<<sumErr<<std::endl;
*/

//  std::cout<<std::endl<<std::endl;
/*
  for(int i1=0;i1<2*inx;i1++)
  {
    std::cout<<prbs[i1]<<" ";
  }
  std::cout<<std::endl<<std::endl;
*/
  
  for(int i1=0;i1<m_fft_size;i1++)
  {
    m_pilots_conj[i1]=std::conj(m_pilots[i1]);
  }
  
  m_equalized_symbols = new cFloat[m_data_packet_size];
  m_estimated_soft_bits = new float[m_data_packet_size];
  
  /*
  for(int i1 =0;i1<100;i1++)
	    std::cout<<m_pilots[i1]<<" ";
    std::cout<<std::endl;
  */
//  float amp1 = sqrt(1.0/2.0);
//  cFloat  mod[4] = {{amp1,amp1},{amp1,-amp1},{-amp1,-amp1},{-amp1,amp1}};
/*
  m_data = new cFloat[m_fft_size];
  m_data_est = new cFloat[m_fft_size];
  memset( m_data, 0, m_fft_size * sizeof(cFloat));

  m_prbs_data = new int[2*m_fft_size]; 
  lfsr.getPRBSbits(2500*8, &m_prbs_data[0], 2*m_fft_size);
  m_snr = 0;
      
  inx = 0;
  for(int i1 =(m_fft_size-NRBDL*12/2);i1<m_fft_size;i1++)
  	{
  	//m_data[i1] =  cFloat((1-2*prbs[2*inx]),(1-2*prbs[2*inx+1]));
  	m_data[i1]=cFloat(sqrt12*(1-2*m_prbs_data[inx]),sqrt12*(1-2*m_prbs_data[inx]));//*scale1;
 //       std::cout<<m_data[i1]<<" ";
  	inx++;
  	//m_data[i1] = mod[i1%4];
  	}
  for(int i1 =1;i1 < (NRBDL*12/2+1);i1++)
    {
  //    m_data[i1] =  cFloat((1-2*prbs[2*inx]),(1-2*prbs[2*inx+1]));
  	m_data[i1]=cFloat(sqrt12*(1-2*m_prbs_data[inx]),sqrt12*(1-2*m_prbs_data[inx]));//*scale1;
//        std::cout<<m_data[i1]<<" ";
      inx++;
  	// m_data[i1] = mod[i1%4];
    }
*/    
// std::cout<<std::endl<<std::endl;
/*
  for(int i1=0;i1<2*inx;i1++)
  {
    std::cout<<prbs[i1]<<" ";
  }
  std::cout<<std::endl<<std::endl;
*/
    /*
    for(int i1 =0;i1<4;i1++)
	std::cout<<mod[i1] <<" ";
    std::cout<<std::endl;
    */
    /*
    for(int i1 =0;i1<100;i1++)
	    std::cout<<m_data[i1]<<" ";
    std::cout<<std::endl;
    */
    
    //cFloat h1(0.3,10);
    //cFloat h2 = h1;
    //h2 /=10;
    //std::cout<<h1<<" "<<h2<<std::endl;

  };
  
  ~SF_IN()
  {
  
    for(int i1=0;i1<7;i1++)
    {
      delete(m_tmp_tx_subframeT[i1]);
      delete(m_tmp_tx_subframeF[i1]);
      fftwf_destroy_plan(m_fft_SF_TX_F2T[i1]);
      delete(m_tmp_rx_subframeT[i1]);
      delete(m_tmp_rx_subframeF[i1]);
      fftwf_destroy_plan(m_fft_SF_RX_T2F[i1]);
    }
    delete(m_tmp_tx_subframeT);
    delete(m_tmp_tx_subframeF);
    free(m_fft_SF_TX_F2T);
    delete(m_tmp_rx_subframeT);
    delete(m_tmp_rx_subframeF);
    free(m_fft_SF_RX_T2F);
   
    delete(m_pss_symbol); 
    delete(m_pilots);
    delete(m_pilots_conj);
    delete(m_data); 
    delete(m_data_est);  
    delete(m_prbs_pilots);
    delete(m_pilots_locations);
    delete(m_pilots_modulated_values);
    delete(m_data_locations);
    delete(m_equalized_symbols);
    delete(m_estimated_soft_bits);
  
  };

  int processRx(cFloat** w_sf)
  {
    cFloat* tmpRxPilots = new cFloat[NRBDL*12/2];       
    cFloat* tmpRxData = new cFloat[m_data_in_symbol];       
    cFloat* tmpChEst = new cFloat[NRBDL*12];        

    m_PilotsPower =0;
    m_DataPower =0;  

    int inx = 0;
    for(int isym = 0; isym<6; isym++) 
    {
    memset(&tmpRxPilots[0],0,m_data_in_symbol*sizeof(cFloat));          
    memset(&tmpRxData[0],0,m_data_in_symbol*sizeof(cFloat));      
//    memset(&tmpChEst[0],0,2*m_data_in_symbol*sizeof(cFloat));      
    memset(&tmpChEst[0],0,NRBDL*12*sizeof(cFloat));

    // picks from received symbol 2 the pilots and data locations 
    for(int i1 = 0; i1 < m_data_in_symbol; i1++)
    {
      tmpRxPilots[i1] = w_sf[isym][m_pilots_locations[i1]];
      tmpRxData[i1] = w_sf[isym][m_data_locations[i1]];
      m_PilotsPower += std::norm(tmpRxPilots[i1]);
      m_DataPower += std::norm(tmpRxData[i1]);
    }
/*    for(int i1=0;i1<10;i1++)
    {
//      std::cout<<w_sf[isym][m_data_locations[i1]]<<" ";
       std::cout<<tmpRxData[i1]<<" ";
    }
    std::cout<<std::endl;
*/

/*
    std::cout<<PowerData<<" "<<PowerPilots<<" "<<PowerData/PowerPilots<<std::endl;
    if((PowerData/PowerPilots)<0.5)
    {
      memset(&m_equalized_symbols[0],0,m_data_packet_size*sizeof(cFloat));
      memset(&m_estimated_soft_bits[0],0,m_data_packet_size*sizeof(float));
      return 1; 
    }
*/
    // channel estimate
    for(int i1=0; i1 < NRBDL*12/2;i1++)
    {
    	tmpChEst[2*i1] = tmpRxPilots[i1]/m_pilots_modulated_values[i1];
    }
    // approximates channel estimates to the data locations 
    for(int i1=1; i1 < (NRBDL*12-1);i1+=2)
    {
   	tmpChEst[i1].real((tmpChEst[i1-1].real()+tmpChEst[i1+1].real())/2.0);
   	tmpChEst[i1].imag((tmpChEst[i1-1].imag()+tmpChEst[i1+1].imag())/2.0);
    }
    // the last symbol does not have pairs therefore the eq has to computed differently
    tmpChEst[NRBDL*12-1].real(tmpChEst[NRBDL*12-2].real()+(tmpChEst[NRBDL*12-2].real()-tmpChEst[NRBDL*12-4].real()/2.0));
    tmpChEst[NRBDL*12-1].imag(tmpChEst[NRBDL*12-2].imag()+(tmpChEst[NRBDL*12-2].imag()-tmpChEst[NRBDL*12-4].imag()/2.0));

/*
    for(int i1=0;i1<10;i1++)
    {
//      std::cout<<w_sf[isym][m_data_locations[i1]]<<" ";
//       std::cout<<tmpRxData[i1]<<" ";
//         std::cout<<tmpChEst[i1]<<" ";
          std::cout<<tmpRxData[i1]/tmpChEst[2*i1+1]<<" ";
    }
    std::cout<<std::endl;
*/
    float sqrt12= sqrt(1.0/2.0);
    cFloat rotConst = cFloat(sqrt12,-sqrt12);
    // equalization and BPSK soft bit creation
    for(int i1=0; i1< (NRBDL*12/2);i1++)
    {
      // std::cout<<" test1_1\n";
      m_equalized_symbols[inx] = tmpRxData[i1]/tmpChEst[2*i1+1]*rotConst;
      // std::cout<<" test1_2\n";
      m_estimated_soft_bits[inx] = m_equalized_symbols[inx].real();//+m_equalized_symbols[inx].imag();
//      std::cout<<(m_estimated_soft_bits[inx]<0)<<" ";
      inx++;
    }
//    std::cout<<"||"<<std::endl;

    }   
    
    return 0;
  };
/*
  int processRx1(cFloat** w_sf)
  {

    cFloat* tmpRxPilots = new cFloat[NRBDL*12/2];       
    cFloat* tmpRxData = new cFloat[NRBDL*12/2];       
    cFloat* tmpChEst = new cFloat[NRBDL*12];        

    // picks from received symbol 2 the pilots and data locations 
    for(int i1 = 0; i1 < NRBDL*12/2; i1++)
    {
      tmpRxPilots[i1] = w_sf[2][m_pilots_locations[i1]];
      tmpRxData[i1] = w_sf[2][m_data_locations[i1]];
    }

    // channel estimate
    for(int i1=0; i1 < NRBDL*12/2;i1++)
    {
    	tmpChEst[2*i1] = tmpRxPilots[i1]/m_pilots_modulated_values[i1];
    }
    
    // approximates channel estimates to the data locations 
    for(int i1=1; i1 < (NRBDL*12-1);i1+=2)
    {
   	tmpChEst[i1].real((tmpChEst[i1-1].real()+tmpChEst[i1+1].real())/2.0);
   	tmpChEst[i1].imag((tmpChEst[i1-1].imag()+tmpChEst[i1+1].imag())/2.0);
    }
    // the last symbol does not have pairs therefore the eq has to computed differently
    tmpChEst[NRBDL*12-1].real(tmpChEst[NRBDL*12-2].real()+(tmpChEst[NRBDL*12-2].real()-tmpChEst[NRBDL*12-4].real()/2.0));
    tmpChEst[NRBDL*12-1].imag(tmpChEst[NRBDL*12-2].imag()+(tmpChEst[NRBDL*12-2].imag()-tmpChEst[NRBDL*12-4].imag()/2.0));
  

    // equalization and BPSK soft bit creation
    for(int i1=0; i1< (NRBDL*12/2);i1++)
    {
      // std::cout<<" test1_1\n";
      m_equalized_symbols[i1] = tmpRxData[i1]/tmpChEst[2*i1+1];
      // std::cout<<" test1_2\n";
      m_estimated_soft_bits[i1] = m_equalized_symbols[i1].real()+m_equalized_symbols[i1].imag();
    }

    return 0;
  };
*/
 int channelEstiamtion()
 {
 
 return 0;
 };
 
 //////////////////////////////////////////// 
 //
 float* getBPSKSoftBits()
 { 
   return &m_estimated_soft_bits[0];
 };

 cFloat* getEqualizedSymbols()
 { 
   return &m_equalized_symbols[0];
 };
  
 int getDataLength()
 {
   return (m_data_packet_size);
 };

 float getPilotPower()
 {
   return m_PilotsPower;
 };

 float getDataPower()
 {
   return m_DataPower;
 };

public:
  int cell_id = -1;
  int N_id_1 = -1;
  int N_id_2 = -1;
  int NRBDL = -1;
  int m_nr_data_symbols = -1;

  int m_data_in_symbol;
  int m_data_packet_size;

  cFloat* m_pss;
  cFloat* m_pss_symbol;
  cFloat* m_pilots;
  cFloat* m_pilots_conj;
  cFloat* m_ch_estimate;  
  cFloat* m_data;
  cFloat* m_data_soft;

  int* m_data_est;
  
  
  int* m_prbs_pilots;
  int* m_pilots_locations;
  cFloat* m_pilots_modulated_values;
  int* m_data_locations;
  
  float* m_estimated_soft_bits;
  cFloat* m_equalized_symbols;

  float m_PilotsPower;
  float m_DataPower;  


    
  int m_fft_size = 2048;
  int m_subframe_size = 30720;
  int m_spectrum_scaling = 1;
  
  int cp_max[7] = {160,144,144,144,144,144,144}; 	
  int cp[7]; 	

  cFloat **m_tmp_tx_subframeT;
  cFloat **m_tmp_tx_subframeF;
  fftwf_plan *m_fft_SF_TX_F2T;

  cFloat **m_tmp_rx_subframeT;
  cFloat **m_tmp_rx_subframeF;  
  fftwf_plan *m_fft_SF_RX_T2F;
  
  
  std::vector<std::complex<float>> m_tmp_buff;
  std::vector<std::complex<float>*> m_tmp_buffs;

/*
  int m_errorSum =0;
  int m_accum_errors = 0;
  int m_accum_rx_bits = 0;
  double m_snr=0;
*/
};  
