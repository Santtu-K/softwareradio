#pragma once

#include "Headers.hpp"
#include "PSS.hpp"
#include "SSS.hpp"
#include "LFSR.hpp"

#include <iostream>
#include <fstream>
using namespace std;

class SF_OUT
{
public:
  SF_OUT(int w_fft_size,int w_NRBDL,int w_cell_id): m_fft_size(w_fft_size),NRBDL(w_NRBDL),cell_id(w_cell_id)
  {
  // pss 
  N_id_1 = cell_id%3;          // Selects the primary synchronization sequence (PSS)
  N_id_2 = (cell_id-N_id_1)/3; // Selects the secondary synchronization sequence (SSS) - not in use in this version  

  // max fft_size = 2048 
  // for other sizes it is downscaled 
  m_spectrum_scaling = 2048/m_fft_size;
  m_subframe_size = 30720/m_spectrum_scaling/2;

  m_nr_data_symbols = 6;
  m_data_in_symbol = NRBDL*12/2;
  m_data_packet_size = m_data_in_symbol*m_nr_data_symbols;

  // cyclic prefics for this downscaling factor
  for(int i1=0;i1<7;i1++)
    {
  	cp[i1] = cp_max[i1]/m_spectrum_scaling;
    }	
    
  m_tmp_buff.resize(m_subframe_size);
  m_tmp_buffs.push_back(&m_tmp_buff.front());

  // synchronization symbol
  m_pss_symbol = new cFloat[m_fft_size];
  memset( m_pss_symbol, 0, m_fft_size * sizeof(cFloat));
  
  int sync_symbol_size = 62;
  m_pss = new cFloat[ sync_symbol_size ];
  int m_pss_nr = N_id_1;
  PSS::getPSS(m_pss_nr, m_pss);
  std::copy(&m_pss[0],&m_pss[32],&m_pss_symbol[m_fft_size-1-30]);
  std::copy(&m_pss[31],&m_pss[62],&m_pss_symbol[1]);
  
  float scale2 = (NRBDL*12)/62;// /sqrt(2);//*2;
  for(int i1=0;i1<m_fft_size;i1++)
  {
    m_pss_symbol[i1] = m_pss_symbol[i1]*scale2;
  }
  
  // mask for symbol start and end shaping 
  maskLong = new float[cp[0]];
  for(int i1=0;i1<cp[0];i1++)
  {
    maskLong[i1] = (float)exp(-(float)((i1-cp[0]-1)*(i1-cp[0]-1))/(float)((cp[0]-1)*(cp[0]-1)/2/2));
  }
  maskShort = new float[cp[1]];
  std::cout<<cp[1]<<" ";
  for(int i1=0;i1<cp[1];i1++)
  {
    maskShort[i1] = (float) exp((double)(-(((float)(i1-cp[1])-1.0)*((float)(i1-cp[1])-1.0)))/(((float)(cp[1]-1)*(float)(cp[1]-1))/2.0/2.0));
    std::cout<<(double) maskShort[i1] <<" ";
  }
  std::cout<<std::endl;
  
  // prepare subframe structure 
  m_tmp_tx_subframeF = new cFloat*[7];
  m_tmp_tx_subframeT = new cFloat*[7];
  for(int i1 =0;i1<7;i1++)
  {
    m_tmp_tx_subframeF[i1]= new cFloat[m_fft_size];
    m_tmp_tx_subframeT[i1]= new cFloat[m_fft_size];
  }
  
  m_fft_SF_TX_F2T = (fftwf_plan*) malloc(sizeof(fftwf_plan*)*7);
  for(int i1 =0;i1<7;i1++)
  {
    m_fft_SF_TX_F2T[i1] = fftwf_plan_dft_1d(m_fft_size,(fftwf_complex*)&m_tmp_tx_subframeF[i1][0],(fftwf_complex*)&m_tmp_tx_subframeT[i1][0],FFTW_BACKWARD,FFTW_ESTIMATE);
  }

  // prepare subframe structure 
  m_tmp_rx_subframeT = new cFloat*[7];
  m_tmp_rx_subframeF = new cFloat*[7];
  for(int i1 =0;i1<7;i1++)
  {
    m_tmp_rx_subframeF[i1]= new cFloat[m_fft_size];
    m_tmp_rx_subframeT[i1]= new cFloat[m_fft_size];
  }

  m_fft_SF_RX_T2F=(fftwf_plan*) malloc(sizeof(fftwf_plan*)*7);
  for(int i1 =0;i1<7;i1++)
  {
    m_fft_SF_RX_T2F[i1] = fftwf_plan_dft_1d(m_fft_size,(fftwf_complex*)&m_tmp_rx_subframeT[i1][0],(fftwf_complex*)&m_tmp_rx_subframeF[i1][0],FFTW_FORWARD,FFTW_ESTIMATE);
  }
  

  m_pilots = new cFloat[m_fft_size];
  memset( m_pilots, 0, m_fft_size * sizeof(cFloat));
  
//  LFSR_for_GoldSequence lfsr;
  auto lfsr = new LFSR_for_GoldSequence;
  int *prbs = new int[2*m_fft_size];

  float scale1 = (NRBDL*12)/62;  
  float sqrt12= sqrt(1/2.0);


  m_pilots_locations = new int[NRBDL*12/2];
  m_pilots_modulated_values = new cFloat[NRBDL*12/2];
  m_data_locations = new int[NRBDL*12/2];
  
  int inx = 0;
  for(int i1 =(m_fft_size-NRBDL*12/2); i1<m_fft_size; i1+=2)
  {
    m_pilots_locations[inx] = i1;
    m_data_locations[inx] = i1+1;
    m_pilots_modulated_values[inx] = cFloat(sqrt12*(1-2*prbs[2*inx]),sqrt12*(1-2*prbs[2*inx]));
    inx++;
  }
  for(int i1 = 1; i1 < (NRBDL*12/2); i1+=2)
    {
      m_pilots_locations[inx] = i1;
      m_data_locations[inx] = i1+1;
      m_pilots_modulated_values[inx] = cFloat(sqrt12*(1-2*prbs[2*inx]),sqrt12*(1-2*prbs[2*inx]));
      inx++;
    }

  lfsr->getPRBSbits(1300*8, &prbs[0], 2*m_fft_size);
  
  
  // Preallocate the pilots into default symbol
  inx =0;
  for(int i1 =(m_fft_size-NRBDL*12/2);i1<m_fft_size;i1++)
  	{
  	m_pilots[i1] = cFloat(sqrt12*(1-2*prbs[inx]),sqrt12*(1-2*prbs[inx]));//*scale1;
  	inx++;
  	//m_pilots[i1].real(-2.0*(i1%2)+1.0);
  	}
  for(int i1 =1;i1 < (NRBDL*12/2+1);i1++)
  	{
  	m_pilots[i1] = cFloat(sqrt12*(1-2*prbs[inx]),sqrt12*(1-2*prbs[inx]));//*scale1;
  	inx++;
  	//m_pilots[i1].real(-2.0*(i1%2)+1.0);
  	}
  
  delete lfsr;
  delete prbs;
  };
  
  ~SF_OUT()
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
    delete(m_pss);
    delete(m_pilots);
//    delete(m_data);    
    
    delete(m_pilots_locations);
    delete(m_pilots_modulated_values);
    delete(m_data_locations);

    delete(maskLong);
    delete(maskShort);     
    
    m_tmp_buff.clear();
    m_tmp_buffs.clear();
  };
  



  int prepareTx(std::vector<std::complex<float>> &w_buffer, std::vector<int> &w_data)
  {

    // check that the data fits into available symbols
    // currently only one symbol with every second position
    int nr_symbols = 6; 
    cFloat complexZero = cFloat(0.0,0.0);
    bool signalPresent = true;
    if(w_data.size()==0)
    {
       signalPresent = false;
    }
    else if(w_data.size()!= NRBDL*12/2*nr_symbols)
    {
    	std::cout<<" data has a wrong size it is:"<<w_data.size()<<" should be:"<<NRBDL*12/2*nr_symbols<<std::endl;
    	return 1;
    }

    // modulate
    // currently only bpsk
    float sqrt12= sqrt(1/2.0);
    cFloat* mod_data = new cFloat[w_data.size()];

    if(signalPresent==true)
    {
      for(int i1=0;i1<w_data.size();i1++)
      {
//      std::cout<<w_data[i1]<<" ";
    	  mod_data[i1] = cFloat(sqrt12*(1-2*w_data[i1]),sqrt12*(1-2*w_data[i1]));
      }
    }
    // std::cout<<std::endl;

    // map to the data locations
    int inx=0;
    cFloat* tmpSym = new cFloat[m_fft_size];  
    for(int isym = 0; isym < 6; isym++)
    {
      memset(tmpSym,0,m_fft_size*sizeof(cFloat));      
      for(int i1=0;i1<m_data_in_symbol;i1++)
      { 
        tmpSym[m_pilots_locations[i1]] = m_pilots[m_pilots_locations[i1]];
      }

      if(signalPresent==true)
        {
          for(int i1=0;i1<m_data_in_symbol;i1++)
            {
              tmpSym[m_data_locations[i1]]   = mod_data[inx];
              inx++; 
            }
        } 
      else
        {
          for(int i1=0;i1<m_data_in_symbol;i1++)
            {
              tmpSym[m_data_locations[i1]]   = complexZero;
              inx++;
            }
        }
        /*
        if(i1%2==0)
          tmpSym[m_data_locations[i1]]=cFloat(-sqrt12,-sqrt12); //mod_data[inx];
        else
          tmpSym[m_data_locations[i1]]=cFloat(sqrt12,sqrt12); //mod_data[inx];
        */
        //std::cout<<w_data[inx]<<" "<<int(mod_data[inx].real()<0) <<"|";
      //  inx++;
      
      std::copy(&tmpSym[0],&tmpSym[m_fft_size],&m_tmp_tx_subframeF[isym][0]);
      // std::cout<<"txsig:"<<std::endl;
    }
    delete(tmpSym);
    // std::cout<<std::endl;

// old allocation
/*   
    for(int isym = 0; isym < 6; isym++)
    {

      for(int i1 =(m_fft_size-NRBDL*12/2+1);i1<m_fft_size;i1+=2)
    	{
  	    m_pilots[i1] = cFloat(-sqrt12,-sqrt12); //mod_data[inx];
  	    inx++;
  	    // m_pilots[i1].real(-2.0*(i1%2)+1.0);
  	  }
      for(int i1 =2;i1 < (NRBDL*12/2+1);i1+=2)
  	  {
  	    m_pilots[i1] =  cFloat(sqrt12,sqrt12); //mod_data[inx];
  	    inx++;
  	    // m_pilots[i1].real(-2.0*(i1%2)+1.0);
  	  }

      std::copy(&m_pilots[0],&m_pilots[m_fft_size],&m_tmp_tx_subframeF[isym][0]);
  
    }
*/


/*
    // map to the data locations
    // int 
    inx=0;
    for(int i1 =(m_fft_size-NRBDL*12/2+1);i1<m_fft_size;i1+=2)
  	{
  	m_pilots[i1] = mod_data[inx];
  	inx++;
  	//m_pilots[i1].real(-2.0*(i1%2)+1.0);
  	}
    for(int i1 =2;i1 < (NRBDL*12/2+1);i1+=2)
  	{
  	m_pilots[i1] = mod_data[inx];
  	inx++;
  	//m_pilots[i1].real(-2.0*(i1%2)+1.0);
  	}
*/
    delete(mod_data);
  
    // std::cout<<"generation of the TX symbol "<<std::endl;
    // count starts from 0
    //pilot into symbol 1 into symbol  
/*    
    memset(&m_tmp_tx_subframeF[0][0],0,m_fft_size*sizeof(cFloat));
    //memset(&m_tmp_tx_subframeF[1][0],0,m_fft_size*sizeof(cFloat));
    //memset(&m_tmp_tx_subframeF[2][0],0,m_fft_size*sizeof(cFloat));
    memset(&m_tmp_tx_subframeF[3][0],0,m_fft_size*sizeof(cFloat));
    memset(&m_tmp_tx_subframeF[4][0],0,m_fft_size*sizeof(cFloat));
    memset(&m_tmp_tx_subframeF[5][0],0,m_fft_size*sizeof(cFloat));
*/    


    //data into symbol 2 into symbol   
//    std::copy(&m_pilots[0],&m_pilots[m_fft_size],&m_tmp_tx_subframeF[1][0]);
    //pilot into symbol 3 into symbol   
    //std::copy(&m_data[0],&m_data[m_fft_size],&m_tmp_tx_subframeF[3][0]);    
//    std::copy(&m_pilots[0],&m_pilots[m_fft_size],&m_tmp_tx_subframeF[2][0]);


    // pss 
    std::copy(&m_pss_symbol[0],&m_pss_symbol[m_fft_size],&m_tmp_tx_subframeF[6][0]);
//    memset(&m_tmp_tx_subframeF[6][0],0,m_fft_size*sizeof(cFloat));
    
    
    for(int isym = 0;isym<7;isym++)
    {
      fftwf_execute(m_fft_SF_TX_F2T[isym]);
    }
    // create a time symbol 
    if(w_buffer.size()!=(m_subframe_size+cp[6]))
    	w_buffer.resize(m_subframe_size+cp[6]);
    memset(&w_buffer[0],0,(m_subframe_size+cp[6])*sizeof(cFloat));


/*
    for(int isym=0;isym<7;isym++)
    {
      for(int i2=0;i2<10;i2++)
      {
        std::cout<<m_tmp_tx_subframeT[isym][i2]<<" ";
      }
        std::cout<<std::endl;
    }
*/
    /*	
    {
    double tmpAve = 0;
    float tmpMax = 0;
    float tmpMax2 = 0;
    for(int i2=0;i2<m_fft_size;i2++)
    {
    	// tmpAve += std::abs(w_buffer[i2])*std::abs(w_buffer[i2]);
    	if(tmpMax< (float) std::abs(m_tmp_tx_subframeT[0][i2]))
    	  {
          tmpMax = (float) std::abs(m_tmp_tx_subframeT[0][i2]);
          tmpMax2 = std::norm(m_tmp_tx_subframeT[0][i2]);
        }
    }
    tmpMax = tmpMax*0.95;
    // tmpAve = sqrt(tmpAve);    

    std::cout<<w_buffer.size()<<" aver power:"<<tmpAve<< " "<< (float) tmpMax <<" "<< (float) tmpMax2 <<std::endl;

      for(int i1=0;i1<cp[0];i1++)
      {
       std::cout<<(maskLong[i1])<<" ";
       }
       std::cout<<std::endl;

      for(int i2=0;i2<cp[0];i2++)
      {
       std::cout<<m_tmp_tx_subframeT[0][m_fft_size-cp[0]+i2]*maskLong[i2]<<" ";
       }
       std::cout<<std::endl;
      }
      */

//    for(int i1=0;i1<10;i1++)
//    {
//    	std::cout<<m_tmp_tx_subframeT[2][i1]<<" "<<m_tmp_tx_subframeT[3][i1]<<"|";
//    }    	
//    std::cout<<std::endl;
    int loc_in_sf = 0;
    for(int i1 = 0;i1<7;i1++)
    {
    
    	// std::copy(&m_tmp_tx_subframeT[i1][m_fft_size-cp[i1]],&m_tmp_tx_subframeT[i1][m_fft_size],&w_buffer[loc_in_sf]);
    	
      //float tmpMax3 = 0;
    	if(i1==0)
    	{
    	for(int i2=0;i2<cp[i1];i2++)
    	{
    	  w_buffer[loc_in_sf+i2]= w_buffer[loc_in_sf+i2]
    	      +m_tmp_tx_subframeT[i1][m_fft_size-cp[i1]+i2]*maskLong[i2];
    	      
    	  w_buffer[loc_in_sf+m_fft_size+cp[i1]+i2] = m_tmp_tx_subframeT[i1][i2]*maskLong[cp[i1]-i2-1];
      //  if(tmpMax3< (float) std::norm(w_buffer[loc_in_sf+i2]))
      //    tmpMax3 = (float) std::norm(w_buffer[loc_in_sf+i2]);
          
    	}
      // std::cout<<tmpMax3<<" ";
    	}
    	else
    	{
    	for(int i2=0;i2<cp[i1];i2++)
    	{
    	  w_buffer[loc_in_sf+i2]= w_buffer[loc_in_sf+i2]
    	      +m_tmp_tx_subframeT[i1][m_fft_size-cp[i1]+i2]*maskShort[i2];
    	  w_buffer[loc_in_sf+m_fft_size+cp[i1]+i2] = m_tmp_tx_subframeT[i1][i2]*maskShort[cp[i1]-i2-1];
      //  if(tmpMax3< (float) std::norm(w_buffer[loc_in_sf+i2]))
      //    tmpMax3 = (float) std::norm(w_buffer[loc_in_sf+i2]);
    	}    	
      // std::cout<<tmpMax3<<" ";
    	}
    	std::copy(&m_tmp_tx_subframeT[i1][0],&m_tmp_tx_subframeT[i1][m_fft_size],&w_buffer[loc_in_sf+cp[i1]]);
	
      /*
      for(int i2=0;i2<m_fft_size;i2++)
       {
        if(tmpMax3< (float) std::norm(w_buffer[loc_in_sf+cp[i1]+i2]))
          tmpMax3 = (float) std::norm(w_buffer[loc_in_sf+cp[i1]+i2]);
       }
       std::cout<<tmpMax3<<std::endl;
      */

	/*for(int i2=0;i2<cp[i1];i2++)
    	{
    	  w_buffer[loc_in_sf+m_fft_size+cp[i1]+i2]= w_buffer[loc_in_sf+cp[i1]-i2];
    	}*/
	/*
       for(int i2 =0;i2<100;i2++)
	    std::cout<<w_buffer[loc_in_sf+i2]<<" ";
    	std::cout<<std::endl;
	*/

    	loc_in_sf += cp[i1]+m_fft_size;
    	//std::cout<<loc_in_sf<<" ";
    }
    // std::cout<<std::endl;

    double tmpAve = 0;
    float tmpMax = 0;
//    float tmpMax2 = 0;
    for(int i2=0;i2<w_buffer.size();i2++)
    {
    	// tmpAve += std::abs(w_buffer[i2])*std::abs(w_buffer[i2]);
    	if(tmpMax< (float) std::abs(w_buffer[i2]))
    	  {
          tmpMax = (float) std::abs(w_buffer[i2]);
//          tmpMax2 = std::norm(w_buffer[i2]);
        }
    }
    tmpMax = tmpMax*0.95;
    // tmpAve = sqrt(tmpAve);    
//    std::cout<<w_buffer.size()<<" "<<loc_in_sf<<" aver power:"<<tmpAve<< " "<< (float) tmpMax <<" "<< (float) tmpMax2 <<std::endl;
    
    // tmpAve = 0;
    // double tmpMax2 = 0;
  for(int i2=0;i2<w_buffer.size();i2++)
    {
      w_buffer[i2]/=tmpMax;
      // tmpAve += std::abs(w_buffer[i2])*std::abs(w_buffer[i2]);
      // if(tmpMax2<std::abs(w_buffer[i2]))
      //	  tmpMax2=std::abs(w_buffer[i2]);
    }
    // tmpAve = sqrt(tmpAve);    
    //std::cout<<w_buffer.size()<<" "<<loc_in_sf<<" aver power:"<<tmpAve<< " "<<tmpMax2 <<std::endl;
    
/*    for(int i2 =0;i2<100;i2++)
	    std::cout<<w_buffer[1648+i2]<<" ";
    	std::cout<<std::endl;
*/
    return 0;
    
  };
  
  int processRX()
  {

    return 0;  
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
  cFloat* m_data;

  int* m_pilots_locations;
  cFloat* m_pilots_modulated_values;
  int* m_data_locations;
  
  
  int m_fft_size = 2048;
  int m_subframe_size = 30720;
  int m_spectrum_scaling = 1;
  
  int cp_max[7] = {160,144,144,144,144,144,144}; 	
  int cp[7]; 	
  
  float *maskLong;
  float *maskShort;

  cFloat **m_tmp_tx_subframeT;
  cFloat **m_tmp_tx_subframeF;
  fftwf_plan *m_fft_SF_TX_F2T;

  cFloat **m_tmp_rx_subframeT;
  cFloat **m_tmp_rx_subframeF;  
  fftwf_plan *m_fft_SF_RX_T2F;
  
  
  std::vector<std::complex<float>> m_tmp_buff;
  std::vector<std::complex<float>*> m_tmp_buffs;

  
};

