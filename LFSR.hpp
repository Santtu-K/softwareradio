/***********************************************************
*
* Pseudo random number generator as defined in 36.211 7.2
*
************************************************************/
#pragma once

#include "Headers.hpp"

/*
* usage 
* init to number (in spec sometimes used value 1600)
* run getBits to get w_size amount of values 
*/ 

class LFSR_for_GoldSequence{
	public:
	LFSR_for_GoldSequence()
	:m_index(0), m_init(0), m_var1(1), m_var2(0)
	{
	};

	~LFSR_for_GoldSequence(){
	};
	
	
	void getPRBSbits(unsigned int w_init,int* data, int w_size)
	{
		this->initTo(w_init);
		
		this->getBits(data, w_size);
		
	};

	
	void initTo(unsigned int w_init)
	{
	
		if(w_init<31)
		{
				throw std::invalid_argument("LFSR_for_GoldSequence initialization shouldd have at least length 31");
		}
	
	int Nc = 1600;

	m_var1 = 1;
	m_var2 = w_init;
	unsigned long int tmp1, tmp2;
	// rund throguh Nc samples to get into initial step 
	for(unsigned int i1 = 0; i1 < (Nc-31); i1 ++)
	{
		tmp1 = ((m_var1 & 1) ^ ((m_var1 >> 3) & 1));
		m_var1 ^= (tmp1 << 31);
		m_var1 >>= 1;
		tmp2 = ((m_var2 & 1) ^ ((m_var2 >> 1) & 1) ^ ((m_var2 >> 2) & 1) ^ ((m_var2 >> 3) & 1));
		m_var2 ^= (tmp2 << 31);
		m_var2 >>= 1;
	}
	}
	void getBits(int* data, int w_size)
	{
		
	m_size = w_size;
	unsigned char *m_bits = new unsigned char[w_size];
	
	unsigned long int tmp1, tmp2, tmp_bit;
	// generates the actual samples 
	for(unsigned int i1 = 0; i1 < (m_size - 1); i1++)
	{
		tmp1 = ((m_var1 & 1) ^ ((m_var1 >> 3) & 1));
		m_var1 ^= (tmp1 << 31);
		m_var1 >>= 1;
		tmp2 = ((m_var2 & 1) ^ ((m_var2 >> 1) & 1) ^ ((m_var2 >> 2) & 1) ^ ((m_var2 >> 3) & 1));
		m_var2 ^= (tmp2 << 31);
		m_var2 >>= 1;		
		tmp_bit = ((tmp1&1)^(tmp2&1)&1);
		//std::cout<<(unsigned long int)tmp2<<" ";
		data[i1]=(int)tmp_bit;
//		std::cout<<(int)tmp_bit<<" ";
	}
//	std::cout<<std::endl;

	m_index = 0;
/* 
	std::ofstream myfile3;
	myfile3.open("PSS.dat", std::ios::binary | std::ios::out);	
	//myfile.write((char*)&dl_buff_collected[0],sizeof(cFloat)*tmpSamps);
	myfile3.write((char*)&m_syncInF[0],sizeof(cFloat)*m_fftSize);
	myfile3.close(); 
*/

	};
	
	public:
	unsigned int m_size;
	unsigned long int m_init;
	unsigned long int m_var1, m_var2;
	unsigned int m_index;
	
};
