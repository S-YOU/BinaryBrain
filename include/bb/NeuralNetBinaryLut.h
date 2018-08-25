// --------------------------------------------------------------------------
//  Binary Brain  -- binary neural net framework
//
//                                     Copyright (C) 2018 by Ryuji Fuchikami
//                                     https://github.com/ryuz
//                                     ryuji.fuchikami@nifty.com
// --------------------------------------------------------------------------



#pragma once

#include <array>
#include <vector>
#include <intrin.h>
#include <omp.h>
#include <ppl.h>
#include "NeuralNetLayer.h"
#include "ShuffleSet.h"

namespace bb {

// LUT�������N���X
template <bool feedback_bitwise = false, typename T = float, typename INDEX = size_t>
class NeuralNetBinaryLut : public NeuralNetLayer<T, INDEX>
{
protected:
	INDEX					m_mux_size = 1;
	INDEX					m_frame_size = 1;
	INDEX					m_input_node_size;
	INDEX					m_output_node_size;

public:
	// LUT����̒�`
	virtual int   GetLutInputSize(void) const = 0;
	virtual int   GetLutTableSize(void) const = 0;
	virtual void  SetLutInput(INDEX node, int input_index, INDEX input_node) = 0;
	virtual INDEX GetLutInput(INDEX node, int input_index) const = 0;
	virtual void  SetLutTable(INDEX node, int bit, bool value) = 0;
	virtual bool  GetLutTable(INDEX node, int bit) const = 0;
	
	void InitializeCoeff(std::uint64_t seed)
	{
		std::mt19937_64                     mt(seed);
		std::uniform_int_distribution<int>	rand(0, 1);

		INDEX node_size = GetOutputNodeSize();
		int   lut_input_size = GetLutInputSize();
		int   lut_table_size = GetLutTableSize();

		ShuffleSet	ss(GetInputNodeSize(), mt());
		for (INDEX node = 0; node < node_size; ++node) {
			// ���͂������_���ڑ�
			auto random_set = ss.GetRandomSet(GetLutInputSize());
			for (int i = 0; i < lut_input_size; ++i) {
				SetLutInput(node, i, random_set[i]);
			}

			// LUT�e�[�u���������_���ɏ�����
			for (int i = 0; i < lut_table_size; i++) {
				SetLutTable(node, i, rand(mt) != 0);
			}
		}
	}

	void  SetMuxSize(INDEX mux_size) { m_mux_size = mux_size; }
	INDEX GetMuxSize(void)           { return m_mux_size; }

	virtual void Resize(INDEX input_node_size, INDEX output_node_size)
	{
		m_input_node_size = input_node_size;
		m_output_node_size = output_node_size;
	}

public:
	void  SetBatchSize(INDEX batch_size) { m_frame_size = batch_size * m_mux_size; }

	INDEX GetInputFrameSize(void) const { return m_frame_size; }
	INDEX GetInputNodeSize(void) const { return m_input_node_size; }
	INDEX GetOutputFrameSize(void) const { return m_frame_size; }
	INDEX GetOutputNodeSize(void) const { return m_output_node_size; }

	int   GetInputValueDataType(void) const { return BB_TYPE_BINARY; }
	int   GetInputErrorDataType(void) const { return BB_TYPE_BINARY; }
	int   GetOutputValueDataType(void) const { return BB_TYPE_BINARY; }
	int   GetOutputErrorDataType(void) const { return BB_TYPE_BINARY; }

protected:
	virtual void ForwardNode(INDEX node)
	{
		auto in_buf = GetInputValueBuffer();
		auto out_buf = GetOutputValueBuffer();
		int   lut_input_size = GetLutInputSize();

		for (INDEX frame = 0; frame < m_frame_size; ++frame) {
			int index = 0;
			int mask = 1;
			for (int i = 0; i < lut_input_size; i++) {
				INDEX input_node = GetLutInput(node, i);
				bool input_value = in_buf.Get<bool>(frame, input_node);
				index |= input_value ? mask : 0;
				mask <<= 1;
			}
			bool output_value = GetLutTable(node, index);
			out_buf.Set<bool>(frame, node, output_value);
		}
	}

public:
	virtual void Forward(void)
	{
		INDEX node_size = GetOutputNodeSize();
		int   lut_input_size = GetLutInputSize();
		concurrency::parallel_for<INDEX>(0, node_size, [&](INDEX node)
		{
			ForwardNode(node);
#if 0
			auto in_buf = GetInputValueBuffer();
			auto out_buf = GetOutputValueBuffer();

			for (INDEX frame = 0; frame < m_frame_size; ++frame) {
				int bit = 0;
				int msk = 1;
				for (int i = 0; i < lut_input_size; i++) {
					INDEX input_node = GetLutInput(node, i);
					bool input_value = in_buf.Get<bool>(frame, input_node);
					bit |= input_value ? msk : 0;
					msk <<= 1;
				}
				bool output_value = GetLutTable(node, bit);
				out_buf.Set<bool>(frame, node, output_value);
			}
#endif
		});
	}

	void Backward(void)
	{
	}

	void Update(double learning_rate)
	{
	}


protected:
	inline int GetLutInputIndex(NeuralNetBuffer<T, INDEX>& buf, int lut_input_size, INDEX frame, INDEX node)
	{
		// ���͒l�쐬
		int index = 0;
		int mask = 1;
		for (int i = 0; i < lut_input_size; ++i) {
			INDEX input_node = GetLutInput(node, i);
			index |= (buf.Get<bool>(frame, input_node) ? mask : 0);
			mask <<= 1;
		}
		return index;
	}


	// feedback
	bool								m_feedback_busy = false;
	INDEX								m_feedback_node;
	int									m_feedback_bit;
	int									m_feedback_phase;
	std::vector< std::vector<int> >		m_feedback_input;
	std::vector<double>					m_feedback_loss;
	
	// ���͂��W�v����LUT�P�ʂŊw�K
	inline bool FeedbackLutwise(const std::vector<double>& loss)
	{
		auto in_buf = GetInputValueBuffer();
		auto out_buf = GetOutputValueBuffer();

		INDEX node_size = GetOutputNodeSize();
		INDEX frame_size = GetOutputFrameSize();
		int lut_input_size = GetLutInputSize();
		int	lut_table_size = GetLutTableSize();

		// ����ݒ�
		if (!m_feedback_busy) {
			m_feedback_busy = true;
			m_feedback_node = 0;
			m_feedback_phase = 0;
			m_feedback_loss.resize(lut_table_size);

			m_feedback_input.resize(node_size);
			for (INDEX node = 0; node < node_size; ++node) {
				m_feedback_input[node].resize(frame_size);
				for (INDEX frame = 0; frame < frame_size; ++frame) {
					// ���͒l�쐬
					int value = 0;
					int mask = 1;
					for (int i = 0; i < lut_input_size; ++i) {
						INDEX input_node = GetLutInput(node, i);
						value |= (in_buf.Get<bool>(frame, input_node) ? mask : 0);
						mask <<= 1;
					}
					m_feedback_input[node][frame] = value;
				}
			}
		}

		// ����
		if (m_feedback_node >= node_size) {
			m_feedback_busy = false;
			return false;
		}

		if ( m_feedback_phase == 0 ) {
			// ���ʂ��W�v
			std::fill(m_feedback_loss.begin(), m_feedback_loss.end(), (T)0.0);
			for (INDEX frame = 0; frame < frame_size; ++frame) {
				int lut_input = m_feedback_input[m_feedback_node][frame];
				m_feedback_loss[lut_input] += loss[frame];
			}

			// �o�͂𔽓]
			for (INDEX frame = 0; frame < frame_size; ++frame) {
				out_buf.Set<bool>(frame, m_feedback_node, !out_buf.Get<bool>(frame, m_feedback_node));
			}

			m_feedback_phase++;
		}
		else {
			// ���]���������ʂ��W�v
			for (INDEX frame = 0; frame < frame_size; ++frame) {
				int lut_input = m_feedback_input[m_feedback_node][frame];
				m_feedback_loss[lut_input] -= loss[frame];
			}

			// �W�v���ʂɊ�Â���LUT���w�K
			int	lut_table_size = GetLutTableSize();
			for (int bit = 0; bit < lut_table_size; ++bit) {
				if (m_feedback_loss[bit] > 0 ) {
					SetLutTable(m_feedback_node, bit, !GetLutTable(m_feedback_node, bit));
				}
			}

			// �w�K����LUT�ŏo�͂��Čv�Z
			ForwardNode(m_feedback_node);

			// ����LUT�ɐi��
			m_feedback_phase = 0;
			++m_feedback_node;
		}

		return true;	// �ȍ~���Čv�Z���Čp��
	}

	// �r�b�g�P�ʂŊw�K
	inline bool FeedbackBitwise(const std::vector<double>& loss)
	{
		auto in_buf = GetInputValueBuffer();
		auto out_buf = GetOutputValueBuffer();

		INDEX node_size = GetOutputNodeSize();
		INDEX frame_size = GetOutputFrameSize();
		int lut_input_size = GetLutInputSize();
		int	lut_table_size = GetLutTableSize();

		// ����ݒ�
		if (!m_feedback_busy) {
			m_feedback_busy = true;
			m_feedback_node = 0;
			m_feedback_bit  = 0;
			m_feedback_phase = 0;
			m_feedback_loss.resize(1);
		}

		// ����
		if (m_feedback_node >= node_size) {
			m_feedback_busy = false;
			return false;
		}

		// �����W�v
		double loss_sum = (T)0;
		for (auto v : loss) {
			loss_sum += v;
		}

		if (m_feedback_phase == 0) {
			// ������ۑ�
			m_feedback_loss[0] = loss_sum;

			// �Y��LUT�𔽓]
			SetLutTable(m_feedback_node, m_feedback_bit, !GetLutTable(m_feedback_node, m_feedback_bit));

			// �ύX����LUT�ōČv�Z
			ForwardNode(m_feedback_node);

			++m_feedback_phase;
		}
		else {
			// �������r
			m_feedback_loss[0] -= loss_sum;
			if (m_feedback_loss[0] < 0) {
				// ���]�����Ȃ������悯��Ό��ɖ߂�
				SetLutTable(m_feedback_node, m_feedback_bit, !GetLutTable(m_feedback_node, m_feedback_bit));

				// �ύX����LUT�ōČv�Z
				ForwardNode(m_feedback_node);
			}

			// ����bit�ɐi��
			m_feedback_phase = 0;
			++m_feedback_bit;

			if (m_feedback_bit >= lut_table_size) {
				// ����bitLUT�ɐi��
				m_feedback_bit = 0;
				++m_feedback_node;
			}
		}

		return true;	// �ȍ~���Čv�Z���Čp��
	}


public:
	bool Feedback(const std::vector<double>& loss)
	{
		if (feedback_bitwise) {
			return FeedbackBitwise(loss);
		}
		else {
			return FeedbackLutwise(loss);
		}
	}


public:
	// �o�͂̑����֐�
	template <typename LT, int LABEL_SIZE>
	std::vector<double> GetOutputOnehotLoss(std::vector<LT> label)
	{
		auto buf = GetOutputValueBuffer();
		INDEX frame_size = GetOutputFrameSize();
		INDEX node_size  = GetOutputNodeSize();

		std::vector<double> vec_loss_x(frame_size);
		double* vec_loss = &vec_loss_x[0];

		concurrency::parallel_for<INDEX>(0, frame_size, [&](INDEX frame)
		{
			vec_loss[frame] = 0;
			for (size_t node = 0; node < node_size; ++node) {
				if (label[frame / m_mux_size] == (node % LABEL_SIZE)) {
					vec_loss[frame] += (buf.Get<bool>(frame, node) ? -1.0 : +1.0);
				}
				else {
					vec_loss[frame] += (buf.Get<bool>(frame, node) ? +(1.0 / LABEL_SIZE) : -(1.0 / LABEL_SIZE));
				}
			}
		});

		return vec_loss_x;
	}


	// �V���A���C�Y
protected:
	struct LutData {
		std::vector<int>	lut_input;
		std::vector<bool>	lut_table;

		template <class Archive>
		void serialize(Archive &archive, std::uint32_t const version)
		{
			archive(cereal::make_nvp("input", lut_input));
			archive(cereal::make_nvp("table", lut_table));
		}
	};

public:
	template <class Archive>
	void save(Archive &archive, std::uint32_t const version) const
	{
		INDEX node_size = GetOutputNodeSize();
		int lut_input_size = GetLutInputSize();
		int	lut_table_size = GetLutTableSize();

		std::vector<LutData> vec_lut;
		for (INDEX node = 0; node < node_size; ++node) {
			LutData ld;
			ld.lut_input.resize(lut_input_size);
			for (int i = 0; i < lut_input_size; ++i) {
				ld.lut_input.push_back(GetLutInput(node, i));
			}

			ld.lut_table.resize(lut_table_size);
			for (int i = 0; i < lut_table_size; ++i) {
				ld.lut_table.push_back(GetLutTable(node, i));
			}

			vec_lut.push_back(ld);
		}

		archive(cereal::make_nvp("lut", vec_lut));
	}
	

	template <class Archive>
	void load(Archive &archive, std::uint32_t const version)
	{
		INDEX node_size = GetOutputNodeSize();
		int lut_input_size = GetLutInputSize();
		int	lut_table_size = GetLutTableSize();

		std::vector<LutData> vec_lut;
		archive(cereal::make_nvp("lut", vec_lut));

		for (INDEX node = 0; node < node_size; ++node) {
			for (int i = 0; i < lut_input_size; ++i) {
				SetLutInput(node, i, ld.lut_input[i]);
			}

			ld.lut_table.resize(lut_table_size);
			for (int i = 0; i < lut_table_size; ++i) {
				SetLutTable(node, i, ld.lut_input[i]);
			}
		}
	}
};

}

