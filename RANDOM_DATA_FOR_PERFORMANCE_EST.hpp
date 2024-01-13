/**************************************************************
*
* function generating known data in tx side 
* testing it in rx side
*
***************************************************************/


#pragma once
#include "Headers.hpp"
#include <iostream>
#include <fstream>

#include "LFSR.hpp"


class RANDOM_DATA_FOR_PERFORMANCE_EST
{
public:
  // generates data from pseudo random sequence 
  // 
  RANDOM_DATA_FOR_PERFORMANCE_EST(int  w_fft_size, int w_NRBDL): m_fft_size(w_fft_size), NRBDL(w_NRBDL)
  {
  
    m_errorSum =0;
    m_accum_errors = 0;
    m_accum_rx_bits = 0;

    m_data_lenght = NRBDL*12/2*6;
    
    prbs = new int[2*m_data_lenght];
    
    lfsr = new LFSR_for_GoldSequence;

    lfsr->getPRBSbits(1300*8, &prbs[0], 2*m_data_lenght);

  
    m_data.clear();
  //  std::cout<<"m_data: ";

    for(int i1 = 0; i1<(m_data_lenght);i1++)
    {
      m_data.push_back(prbs[2*i1+1]); // picks every other random value
  //    std::cout<<m_data.at((i1-1)/2)<<" ";
    }  
  //    std::cout<<std::endl;

  };
  
  ~RANDOM_DATA_FOR_PERFORMANCE_EST()
  {
  
    m_data.clear();
    delete(prbs);
    delete(lfsr);   
  };
  
  // this function is normally not used 
  // 
  int generateDataToTransmit()
  {
    
    lfsr->getPRBSbits(1300*8, &prbs[0], 2*m_data_lenght);
    m_data.clear();
    for(int i1 = 0; i1<m_data_lenght;i1++)
    {
      m_data.push_back(prbs[2*i1+1]); // picks every other random value
      //  std::cout<<tx_data.at((i1-1)/2)<<" ";
    }  

    return 0;
  };
  
  // computes statistics
  // 
  int processRxData(float* w_soft_bits, cFloat* w_rx_symbols,int w_data_size)
  {
  
    float* estEVM = new float[(int)(m_data_lenght)];
    
    m_errorSum = 0;
    for(int i1 = 0;i1<m_data_lenght;i1++)
    {
      m_errorSum +=((int)((w_soft_bits[i1]<0)!=m_data[i1]));
      estEVM[i1] = w_soft_bits[i1]*(1-2*m_data[i1]);
    }

    for(int i1=0;i1<6;i1++)
    {
      int tmpErrSum = 0;
    for(int i2=0;i2<144;i2++)
    {
  /*    if((w_soft_bits[i1*144+i2]<0)!=m_data[i1*144+i2])
        {
          std::cout<<i2<<":"<<(w_soft_bits[i1*144+i2]<0)<<m_data[i1*144+i2]<< " "<<w_soft_bits[i1*144+i2]<<"| ";
        }
      std::cout<<(w_soft_bits[i1*144+i2]<0)<<" ";
  */      if((w_soft_bits[i1*144+i2]<0)!=m_data[i1*144+i2])
          tmpErrSum++;
    }
  // std::cout<<" ErrSum:"<<tmpErrSum<<std::endl;
    }
  //  std::cout<<" error est: "<<m_errorSum<< std::endl;
    
       	
    double accumEVM =0;
    double tmpMean =0;
    for(int i1 = 0;i1<m_data_lenght;i1++)
  	{
  	tmpMean +=estEVM[i1];
  	}
    tmpMean /=m_data_lenght;
    for(int i1 = 0;i1<m_data_lenght;i1++)
  	{
  	  accumEVM += ((estEVM[i1]/tmpMean-1)*(estEVM[i1]/tmpMean-1));
  	}
  	
     m_snr +=(accumEVM/m_data_lenght); 
     m_snr /=2;
	
     m_accum_errors +=m_errorSum;
     m_accum_rx_bits +=m_data_lenght;

     std::cout<<"err in this sf: "<< m_errorSum<<" total err:"<<m_accum_errors<<" bits:"<<m_accum_rx_bits<<" BER:"<<(double)m_accum_errors/(double)(m_accum_rx_bits)<<" BER_dB:"<<log10((double)m_accum_errors/(double)(m_accum_rx_bits)) <<" snr_db:"<<10*log10(1.0/m_snr)<<std::endl;
     
     delete estEVM;
     
      return 0;  
  };
  
public:

  int *prbs;
  LFSR_for_GoldSequence* lfsr;

  std::vector<int> m_data;
  int m_fft_size; // = 2048;
  int NRBDL; // = 24;
  int m_data_lenght; // = NRBDL*12/2;
  
  int m_errorSum;// =0;
  int m_accum_errors;// = 0;
  int m_accum_rx_bits;// = 0;
  double m_snr;
  
};
