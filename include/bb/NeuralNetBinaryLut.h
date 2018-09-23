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
#include "NeuralNetSparseLayer.h"

namespace bb {


// LUT�^�̊�{�N���X
// �͋Z�ł̊w�K������
// �t���b�g�Ȍ����ł���Ώd���������̂ŁALUT�P�ʂœ��v�����Ȃ��牉�Z���\
// ��ݍ��ݎ���bit���Ɍ��ʂɑ��݉e������̂�bit�P�ʂł�邵����������


// LUT�������N���X
template <bool feedback_bitwise = false, typename T = float, typename INDEX = size_t>
class NeuralNetBinaryLut : public NeuralNetSparseLayer<T, INDEX>
{
	typedef NeuralNetSparseLayer<T, INDEX> super;

protected:
	INDEX					m_mux_size = 1;
	INDEX					m_frame_size = 1;

public:
	// LUT����̒�`
	virtual int   GetLutInputSize(void) const = 0;
	virtual int   GetLutTableSize(void) const = 0;
	virtual void  SetLutInput(INDEX node, int bitpos, INDEX input_node) = 0;
	virtual INDEX GetLutInput(INDEX node, int bitpos) const = 0;
	virtual void  SetLutTable(INDEX node, int bitpos, bool value) = 0;
	virtual bool  GetLutTable(INDEX node, int bitpos) const = 0;

	int   GetNodeInputSize(INDEX node) const { return GetLutInputSize(); }
	void  SetNodeInput(INDEX node, int input_index, INDEX input_node) { SetLutInput(node, input_index, input_node);  }
	INDEX GetNodeInput(INDEX node, int input_index) const { return GetLutInput(node, input_index); }


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
	
	template <typename RT, typename RI>
	void ImportLayer(const NeuralNetSparseLayer<RT, RI>& lim)
	{
		auto node_size = GetOutputNodeSize();
		auto input_size = GetLutInputSize();
		auto table_size = GetLutTableSize();

		BB_ASSERT(lim.GetOutputNodeSize() == node_size);
		for (INDEX node = 0; node < node_size; ++node) {
			BB_ASSERT(lim.GetNodeInputSize(node) == input_size);

			// ���͂��R�s�[
			for (int input_index = 0; input_index < input_size; ++input_index) {
				SetLutInput(node, input_index, lim.GetNodeInput(node, input_index));
			}

			// �W�����o�C�i����
			std::vector<T> vec(input_size);
			for (int index = 0; index < table_size; ++index) {
				for (int bit = 0; bit < input_size; ++bit) {
					vec[bit] = (index & (1 << bit)) ? (RT)1.0 : (RT)0.0;
				}
				RT v = lim.CalcNode(node, vec);
				SetLutTable(node, index, (v >= 0));
			}
		}
	}


	void  SetMuxSize(INDEX mux_size) {
		m_mux_size = mux_size;
	}
	
	INDEX GetMuxSize(void) const     { return m_mux_size; }


public:
	bool GetLutInputSignal(INDEX frame, INDEX node, int bitpos) const
	{
		INDEX input_node = GetLutInput(node, bitpos);
		return GetInputSignalBuffer().Get<bool>(frame, input_node);
	}

	virtual int GetLutInputIndex(INDEX frame, INDEX node) const
	{
		const auto& buf = GetInputSignalBuffer();
		int lut_input_size = GetLutInputSize();
		int index = 0;
		int mask = 1;
		for (int bitpos = 0; bitpos < lut_input_size; ++bitpos) {
			INDEX input_node = GetLutInput(node, bitpos);
			if (GetInputSignalBuffer().Get<bool>(frame, input_node)) {
				index |= mask;
			}
			mask <<= 1;
		}
		return index;
	}


	void  SetBatchSize(INDEX batch_size) { m_frame_size = batch_size * m_mux_size; }

	INDEX GetInputFrameSize(void) const { return m_frame_size; }
	INDEX GetOutputFrameSize(void) const { return m_frame_size; }

	int   GetInputSignalDataType(void) const { return BB_TYPE_BINARY; }
	int   GetInputErrorDataType(void) const { return NeuralNetType<T>::type; }
	int   GetOutputSignalDataType(void) const { return BB_TYPE_BINARY; }
	int   GetOutputErrorDataType(void) const { return NeuralNetType<T>::type; }

protected:
	virtual void ForwardNode(INDEX node)
	{
		auto in_sig_buf = GetInputSignalBuffer();
		auto out_sig_buf = GetOutputSignalBuffer();
		int   lut_input_size = GetLutInputSize();

		for (INDEX frame = 0; frame < m_frame_size; ++frame) {
			int index = 0;
			int mask = 1;
			for (int i = 0; i < lut_input_size; i++) {
				INDEX input_node = GetLutInput(node, i);
				bool input_signal = in_sig_buf.Get<bool>(frame, input_node);
				index |= input_signal ? mask : 0;
				mask <<= 1;
			}
			bool output_signal = GetLutTable(node, index);
			out_sig_buf.Set<bool>(frame, node, output_signal);
		}
	}

public:
	virtual void Forward(bool train = true)
	{
		INDEX node_size = GetOutputNodeSize();
		int   lut_input_size = GetLutInputSize();

		#pragma omp parallel for
		for ( int node = 0; node < (int)node_size; ++node) {
			ForwardNode(node);
		}
	}
		
	void Backward(void)
	{
		auto& out_err = GetOutputErrorBuffer();
		auto& in_err = GetInputErrorBuffer();

		INDEX frame_size = GetOutputFrameSize();
		INDEX node_size = GetOutputNodeSize();
		int lut_input_size = GetLutInputSize();
		int lut_table_size = GetLutTableSize();

		// �[��������
		INDEX input_node_size = GetInputNodeSize();
		for (INDEX node = 0; node < input_node_size; ++node) {
			for (INDEX frame = 0; frame < frame_size; ++frame) {
				in_err.Set<T>(frame, node, 0);
			}
		}

		std::mt19937_64 mt(1);

		// �v�Z
		std::vector<T> table_err(lut_table_size);
		for (INDEX node = 0; node < node_size; ++node) {
			std::fill(table_err.begin(), table_err.end(), (T)0);
			for (INDEX frame = 0; frame < frame_size; ++frame) {
				// ���͒l�擾
				int input_index = GetLutInputIndex(frame, node);
				T err = out_err.Get<T>(frame, node);

				// �e�[�u���ɑ΂���덷�v�Z
				table_err[input_index] += err;	// �ώZ���Ă���
			}

			for (int bitpos = 0; bitpos < lut_input_size; ++bitpos) {
				if ( abs(table_err[bitpos]) > (mt() % 16)+5 ) {
					SetLutTable(node, bitpos, table_err[bitpos] > 0);
				}
			}
			
			for (INDEX frame = 0; frame < frame_size; ++frame) {
				int input_index = GetLutInputIndex(frame, node);
				T err = out_err.Get<T>(frame, node);

				bool val = GetLutTable(node, input_index);
				if ((val && err < 0) || (val && err > 0)) {

					// ���͂ɑ΂���`�d�덷�v�Z
					int mask = 1;
			//		for (int bitpos = 0; bitpos < lut_input_size; ++bitpos) {
					{
						int bitpos = (int)(mt() % lut_input_size);

						INDEX input_node = GetLutInput(node, bitpos);
						// �e���͍��ɑ΂���e�[�u���̕Δ������v�Z
						int index0 = (input_index & ~mask);
						int index1 = (input_index | mask);
						bool val0 = GetLutTable(node, index0);
						bool val1 = GetLutTable(node, index1);

						if (!val0 && val1) {
							in_err.Set<T>(frame, input_node, in_err.Get<T>(frame, input_node) + err);
						}
						else if (val0 && !val1) {
							in_err.Set<T>(frame, input_node, in_err.Get<T>(frame, input_node) - err);
						}
						mask <<= 1;
					}
				}
			}

		}
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
	std::mt19937_64						m_feedback_mt;
	bool								m_feedback_busy = false;
	INDEX								m_feedback_node;
	int									m_feedback_bit;
	int									m_feedback_phase;
	std::vector< std::vector<int> >		m_feedback_input;
	std::vector<double>					m_feedback_loss;
	
	// ���͂��W�v����LUT�P�ʂŊw�K
	inline bool FeedbackLutwise(const std::vector<double>& loss)
	{
		auto in_buf = GetInputSignalBuffer();
		auto out_buf = GetOutputSignalBuffer();

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
#if 1
					int value = 0;
					int mask = 1;
					for (int i = 0; i < lut_input_size; ++i) {
						INDEX input_node = GetLutInput(node, i);
						value |= (in_buf.Get<bool>(frame, input_node) ? mask : 0);
						mask <<= 1;
					}
					m_feedback_input[node][frame] = value;
#else
					m_feedback_input[node][frame] = GetLutInputIndex(frame, node);
#endif
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

	// [�A�C�f�A��������]
	// ��ݍ��݂Ńr�b�g�P�ʂ͂��܂肤�܂������Ă��Ȃ�
	// �n�~���O����1�Â����ړ��ł��Ȃ��̂ŋǏ����ɗ����₷���͗l
	// �Ⴆ��4bit���炢���ɂ���16��܂킵�Ĉ�ԗǂ����̂����Ƃ���
	// �K�v�ȋC������B�I��4bit�͈قȂ�LUT���ׂ��̂����肾�Ǝv��

	// �r�b�g�P�ʂŊw�K
	inline bool FeedbackBitwise(const std::vector<double>& loss)
	{
		auto in_buf = GetInputSignalBuffer();
		auto out_buf = GetOutputSignalBuffer();

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

			std::normal_distribution<double> dist(0.0, 0.1);

			if (m_feedback_loss[0] < 0) {
				// ���]�����Ȃ��������ʂ��悯��Ό��ɖ߂�
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
		auto buf = GetOutputSignalBuffer();
		INDEX frame_size = GetOutputFrameSize();
		INDEX node_size  = GetOutputNodeSize();

		std::vector<double> vec_loss_x(frame_size);
		double* vec_loss = &vec_loss_x[0];

		#pragma omp parallel for
		for ( int frame = 0; frame < (int)frame_size; ++frame ) {
			vec_loss[frame] = 0;
			for (size_t node = 0; node < node_size; ++node) {
				if (label[frame / m_mux_size] == (node % LABEL_SIZE)) {
					vec_loss[frame] += (buf.Get<bool>(frame, node) ? 0.0 : +1.0);
				}
				else {
					vec_loss[frame] += (buf.Get<bool>(frame, node) ? +(1.0 / LABEL_SIZE) : -(0.0 / LABEL_SIZE));
				}
			}
		}

		return vec_loss_x;
	}


	// Serialize
protected:
	struct LutData {
		std::vector<INDEX>	lut_input;
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
		archive(cereal::make_nvp("NeuralNetLayer", *(super *)this));

		archive(cereal::make_nvp("input_node_size", m_input_node_size));
		archive(cereal::make_nvp("m_output_node_size", m_output_node_size));

		INDEX node_size = GetOutputNodeSize();
		int lut_input_size = GetLutInputSize();
		int	lut_table_size = GetLutTableSize();

		std::vector<LutData> vec_lut;
		for (INDEX node = 0; node < node_size; ++node) {
			LutData ld;
			ld.lut_input.resize(lut_input_size);
			for (int i = 0; i < lut_input_size; ++i) {
				ld.lut_input[i] = GetLutInput(node, i);
			}

			ld.lut_table.resize(lut_table_size);
			for (int i = 0; i < lut_table_size; ++i) {
				ld.lut_table[i] = GetLutTable(node, i);
			}

			vec_lut.push_back(ld);
		}

		archive(cereal::make_nvp("lut", vec_lut));
	}
	

	template <class Archive>
	void load(Archive &archive, std::uint32_t const version)
	{
		archive(cereal::make_nvp("NeuralNetLayer", *(super *)this));
		
		INDEX input_node_size;
		INDEX output_node_size;
		std::vector<LutData> vec_lut;
		archive(cereal::make_nvp("input_node_size", input_node_size));
		archive(cereal::make_nvp("m_output_node_size", output_node_size));
		archive(cereal::make_nvp("lut", vec_lut));

//		if (vec_lut.empty()) { return; }

		Resize(input_node_size, output_node_size);

		for (INDEX node = 0; node < (INDEX)vec_lut.size(); ++node) {
			for (int i = 0; i < (int)vec_lut[node].lut_input.size(); ++i) {
				SetLutInput(node, i, vec_lut[node].lut_input[i]);
			}

			for (int i = 0; i < (int)vec_lut[node].lut_table.size(); ++i) {
				SetLutTable(node, i, vec_lut[node].lut_table[i]);
			}
		}
	}

	virtual void Save(cereal::JSONOutputArchive& archive) const
	{
		archive(cereal::make_nvp("NeuralNetBinaryLut", *this));
	}

	virtual void Load(cereal::JSONInputArchive& archive)
	{
		archive(cereal::make_nvp("NeuralNetBinaryLut", *this));
	}
};

}

