// --------------------------------------------------------------------------
//  Binary Brain  -- binary neural net framework
//
//                                     Copyright (C) 2018 by Ryuji Fuchikami
//                                     https://github.com/ryuz
//                                     ryuji.fuchikami@nifty.com
// --------------------------------------------------------------------------


#pragma once

#include <vector>
#include <list>
#include <random>
#include <algorithm>


namespace bb {


// �Ȃ�ׂ��d�����Ȃ��悤�Ƀ����_���ɃC���f�b�N�X���V���b�t������
// �g�����v�̃J�[�h��z��C���[�W�ŁA�莝���������Ȃ�΍ď[�U���邱�ƂŁA
// ����̒l�������Əo�Ȃ�������A�������̂��o�����邱�Ƃ�h�~����

// �V���b�t���N���X
class ShuffleSet
{
public:
	ShuffleSet()
	{
	}

	ShuffleSet(size_t size, std::uint64_t seed = 1)
	{
		Setup(size, seed);
	}

	void Setup(size_t size, std::uint64_t seed = 1)
	{
		// ������
		m_mt.seed(seed);
		m_heap.clear();
		m_reserve.clear();

		// �V���b�t��
		std::vector<size_t> heap(size);
		for (size_t i = 0; i < size; i++) {
			heap[i] = i;
		}
		std::shuffle(heap.begin(), heap.end(), m_mt);

		// �ݒ�
		m_heap.assign(heap.begin(), heap.end());
	}

	std::vector<size_t> GetRandomSet(size_t n)
	{
		std::vector<size_t>	set;

		// �w������o��
		for (size_t i = 0; i < n; i++) {
			if (m_heap.empty()) {
				// ��ʂ芄�蓖�Ă��痘�p�ς݂��ė��p
				std::vector<size_t> heap(m_reserve.size());
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
	std::mt19937_64		m_mt;
	std::list<size_t>	m_heap;
	std::list<size_t>	m_reserve;
};


}
