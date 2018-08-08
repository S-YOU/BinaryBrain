

#pragma once

#include <vector>
#include <list>
#include <random>
#include <algorithm>


class ShuffleSet
{
public:
	ShuffleSet()
	{
	}
	
	ShuffleSet(int size, std::uint64_t seed = 1)
	{
		Setup(size, seed);
	}
	
	void Setup(int size, std::uint64_t seed = 1)
	{
		// ������
		m_mt.seed(seed);
		m_heap.clear();
		m_reserve.clear();

		// �V���b�t��
		std::vector<int> heap(size);
		for (int i = 0; i < size; i++) {
			heap[i] = i;
		}
		std::shuffle(heap.begin(), heap.end(), m_mt);

		// �ݒ�
		m_heap.assign(heap.begin(), heap.end());
	}

	std::vector<int> GetSet(int n)
	{
		std::vector<int>	set;

		// �w������o��
		for (int i = 0; i < n; i++) {
			if (m_heap.empty()) {
				// ��ʂ芄�蓖�Ă��痘�p�ς݂��ė��p
				std::vector<int> heap(m_reserve.size());
				heap.assign(m_reserve.begin(), m_reserve.end());
				std::shuffle(heap.begin(), heap.end(), m_mt);
				m_heap.assign(heap.begin(), heap.end());
				m_reserve.clear();
			}

			// �g�������͎̂��O��
			set.push_back(m_heap.front());
			m_heap.pop_front();
		}

		// �g�������̂̓��U�[�u�ɉ�
		for (auto s : set) {
			m_reserve.push_back(s);
		}

		return set;
	}

protected:
	std::mt19937_64	m_mt;	
	std::list<int>	m_heap;
	std::list<int>	m_reserve;
};


