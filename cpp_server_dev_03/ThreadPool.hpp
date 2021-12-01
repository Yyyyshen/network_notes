//Copyright(c) by yyyyshen 2021
#ifndef _THREAD_POOL_HPP
#define _THREAD_POOL_HPP

#include <iostream>
#include <thread>
#include <mutex>
#include <vector>
#include <list>
#include <condition_variable>
#include <memory>


/**
 * ����
 */
class Task final
{
public:
	Task()
	{
		std::cout << "task_ctor" << std::endl;
	}
	~Task()
	{
		std::cout << "task_dtor" << std::endl;
	}
public:
	void do_task()
	{
		std::cout << "task_run" << std::endl;
	}
};

/**
 * �̳߳ؼ��������
 */
class TaskPool final
{
public:								//���ͱ�������
	using this_type = TaskPool;

	using task_type = Task;
	using sptask_type = std::shared_ptr<task_type>;
	using list_type = std::list<sptask_type>;

	using thread_type = std::thread;
	using spthread_type = std::shared_ptr<thread_type>;
	using pool_type = std::vector<spthread_type>;

	using bool_type = bool;
	using lock_type = std::mutex;
	using conv_type = std::condition_variable;

public:								//��������
	TaskPool()
	{
		std::cout << "TaskPool_ctor" << std::endl;
	}
	~TaskPool()
	{
		clear_tasks();
		std::cout << "TaskPool_dtor" << std::endl;
	}
	TaskPool(const this_type&) = delete;
	this_type& operator=(const this_type&) = delete;

public:								//�̳߳�
	void init(int threadNum = 5)
	{
		if (threadNum <= 0)			//����У��
			threadNum = 5;

		m_bRunning = true;

		for (int i = 0; i < threadNum; ++i)
		{
			//spthread_type spThread;
			//spThread.reset(new thread_type(std::bind(&TaskPool::thread_proc, this)));

			auto spThread = std::make_shared<thread_type>(&TaskPool::thread_proc, this);

			m_threadPool.push_back(spThread);
		}
	}
	void stop()
	{
		m_bRunning = false;
		m_cv.notify_all();

		for (auto& spt : m_threadPool)
		{							//�ȴ������߳��˳�
			if (spt->joinable())
				spt->join();
		}
	}

public:								//�������
	void add_task(task_type* task)
	{
		sptask_type spTask;
		spTask.reset(task);

		{							//��С��������
			std::lock_guard<lock_type> guard(m_listMutex);
			m_taskList.push_back(spTask);
		}

		m_cv.notify_one();
	}
	void clear_tasks()
	{
		std::lock_guard<lock_type> guard(m_listMutex);
		for (auto& t : m_taskList)
			t.reset();
		m_taskList.clear();
	}

private:							//ִ������
	void thread_proc()
	{
		sptask_type spTask;
		while (true)
		{
			{						//��С������
				std::unique_lock<std::mutex> guard(m_listMutex);
				while (m_taskList.empty())
				{
					if (!m_bRunning)
						break;

					//����Ϊ�գ���ȴ�
					//
					//�����������߼���
					//�������������������������ͷ������������߳�
					//������notify���̻߳����ٻ����
					m_cv.wait(guard);
				}

				if (!m_bRunning)
					break;

				//ȡ������
				spTask = m_taskList.front();
				m_taskList.pop_front();
			}

			if (spTask == nullptr)
				continue;

			//ִ������
			spTask->do_task();
			spTask.reset();
		}

		std::cout << "tid : " << std::this_thread::get_id() << " thread exit" << std::endl;
	}

private:							//��Ա����������ʱ��ʼ��
	list_type m_taskList;
	lock_type m_listMutex;
	pool_type m_threadPool;
	bool_type m_bRunning = false;
	conv_type m_cv;

};

#endif // !_THREAD_POOL_HPP