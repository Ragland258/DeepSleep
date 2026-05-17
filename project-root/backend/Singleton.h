#pragma once
#include <memory>
#include <mutex>
#include <iostream>
using namespace std;

// 单例模板类
template <class T>
class Singleton
{
protected:
	Singleton() = default;
	Singleton(const Singleton<T>& st) = delete;
	Singleton& operator=(const Singleton<T>& st) = delete;

	static std::shared_ptr<T> _instance;
public:
	~Singleton()
	{
		std::cout << "this is singleton destruct";
	}
	// 获取单例实例
	static std::shared_ptr<T> GetInstance();
	void PrintAddr()
	{
		std::cout << _instance->get();
	}
};

template <class T>
std::shared_ptr<T> Singleton<T>::_instance = nullptr;

template<class T>
inline std::shared_ptr<T> Singleton<T>::GetInstance()
{
	static std::once_flag s_flag;
	std::call_once(s_flag, [&]()
		{
			_instance = std::shared_ptr<T>(new T);
		});
	return _instance;
}
