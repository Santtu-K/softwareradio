#pragma once

#include <queue>
#include <mutex>
#include <memory>
#include <condition_variable>
#include <atomic>
#include <chrono>
#include <string>
#include <map>
#include <utility>

#include <iostream>
#include <thread>
#include <functional>


template<typename T>
class InterThreadQueueSDR7320
{
	public:
	InterThreadQueueSDR7320();

	std::unique_ptr<T> readItem();
	bool writeItem(std::unique_ptr<T> w_item);


	inline bool isAvailable() const noexcept
	{
		return m_itemCount.load(std::memory_order_acquire) > 0;
	}

private:
	std::atomic<unsigned int> m_itemCount;
	std::queue<std::unique_ptr<T>> m_items;
	std::mutex m_itemsMutex;
};

template<typename T>
InterThreadQueueSDR7320<T>::InterThreadQueueSDR7320()
: m_itemCount{0}
{
	static_assert(std::is_move_assignable<T>::value, "T must be move assignable");
}

template<typename T>
std::unique_ptr<T> InterThreadQueueSDR7320<T>::readItem()
{
	// gets lock 
	// if there is no data in the queue waits for w_timeout time 
	// if still no data returns nullptr
	// else returns first data
	std::unique_lock<std::mutex> lock(m_itemsMutex);

	if(m_itemCount.load(std::memory_order_acquire) == 0)
	{
		lock.unlock();
		return nullptr;
	}

	std::unique_ptr<T> retVal = std::move(m_items.front());
	m_items.pop();

	m_itemCount.store(m_itemCount.load(std::memory_order_acquire) - 1, std::memory_order_release);

	lock.unlock();

	return std::move(retVal);
}

template<typename T>
bool InterThreadQueueSDR7320<T>::writeItem(std::unique_ptr<T> w_item)
{
	bool retVal = true;

	if(w_item == nullptr)
	{
		retVal = false;
	}

	if(retVal)
	{
		std::unique_lock<std::mutex> lock{m_itemsMutex};

		m_items.push(std::move(w_item));

		m_itemCount.store(m_itemCount.load(std::memory_order_acquire) + 1, std::memory_order_release);

		lock.unlock();
	}

	return retVal;
}


class TX_META_DATA
{
  public:
  int num_tx_symbols;
};

class TXitem
{
	public:
		TXitem()
		{
		};
		~TXitem(){
			elem.clear();
		};

	  void insertData(std::vector<int> w_data)
	  {
	     elem.clear();
             elem = std::move(w_data);
	  };

		/*
	  void insertData2(std::unique_ptr<std::array<int,792>> w_data)
	  {
			data = std::move(w_data);
	  };
		*/

	public:

        std::vector<int> elem; 
		TX_META_DATA tx_meta_data;
	//std::array<int,792> elem;
};


class RX_META_DATA
{
  public:

  int rx_status;

  float pilot_power;
  float data_power;

  std::vector<cFloat> elem_soft_bits; 
};

class RXitem
{
	public:
		RXitem()
		{
		};
		~RXitem(){
			elem.clear();
		};

	  void insertData(std::vector<float> w_data)
	  {
		elem.clear();
  		elem = std::move(w_data);
	  };
		
	std::vector<float> elem; 
	RX_META_DATA rx_meta_data;
};

/*
//void thread_function(InterthreadQueue<TXitem> &txQueue) //,InterthreadQueue<RXitem> &rxQueue)
void thread_function(InterThreadQueueSDR7320<TXitem> &txQueue,InterThreadQueueSDR7320<RXitem> &rxQueue) //,InterthreadQueue<RXitem> &rxQueue)
{
		// int a=0;
    for(int i = 0; i < 10000; i++);
    auto tmp = std::move(txQueue.readItem());
		while(tmp == nullptr)
			{
				std::this_thread::sleep_for(std::chrono::milliseconds(2) );
				tmp = std::move(txQueue.readItem());
			};
//    std::cout<<"thread function Executing function Pointer "<< (int) tmp.get()[0] << std::endl;
    std::cout<<"thread function Executing function Pointer "<< (int) tmp.get()->elem->get()[1] <<" "<<tmp.get()->elem.size()<< std::endl;


		auto rxItem = std::unique_ptr<RXitem>(new RXitem());

		rxItem->insertData(std::move(tmp.get()->elem));

		rxQueue.writeItem(std::move( rxItem));

		tmp.reset();
		
}
*/
