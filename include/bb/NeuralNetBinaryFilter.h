// --------------------------------------------------------------------------
//  Binary Brain  -- binary neural net framework
//
//                                     Copyright (C) 2018 by Ryuji Fuchikami
//                                     https://github.com/ryuz
//                                     ryuji.fuchikami@nifty.com
// --------------------------------------------------------------------------



#pragma once


#include <vector>
#include <random>
#include <intrin.h>

#include "NeuralNetLayer.h"

namespace bb {


// NeuralNetの抽象クラス
template <typename T = float, typename INDEX = size_t>
class NeuralNetBinaryFilter : public NeuralNetLayerBuf<T, INDEX>
{
	typedef NeuralNetLayerBuf<T, INDEX>	super;

protected:
	NeuralNetLayer<T, INDEX>* m_filter_net;
	INDEX			m_mux_size = 1;
	INDEX			m_frame_size = 1;
	INDEX			m_input_h_size;
	INDEX			m_input_w_size;
	INDEX			m_input_c_size;
	INDEX			m_filter_h_size;
	INDEX			m_filter_w_size;
	INDEX			m_y_step;
	INDEX			m_x_step;
	INDEX			m_output_h_size;
	INDEX			m_output_w_size;
	INDEX			m_output_c_size;

	int				m_feedback_layer = -1;

public:
	NeuralNetBinaryFilter() {}

	NeuralNetBinaryFilter(NeuralNetLayer<T, INDEX>* filter_net, INDEX input_c_size, INDEX input_h_size, INDEX input_w_size, INDEX output_c_size, INDEX filter_h_size, INDEX filter_w_size, INDEX y_step, INDEX x_step)
	{
		SetFilterNet(filter_net);
		Resize(input_c_size, input_h_size, input_w_size, output_c_size, filter_h_size, filter_w_size, y_step, x_step);
	}

	~NeuralNetBinaryFilter() {}		// デストラクタ

	void SetFilterNet(NeuralNetLayer<T, INDEX>* filter_net)
	{
		m_filter_net = filter_net;
	}

	void Resize(INDEX input_c_size, INDEX input_h_size, INDEX input_w_size, INDEX output_c_size, INDEX filter_h_size, INDEX filter_w_size, INDEX y_step, INDEX x_step)
	{
		m_input_c_size = (int)input_c_size;
		m_input_h_size = (int)input_h_size;
		m_input_w_size = (int)input_w_size;
		m_filter_h_size = (int)filter_h_size;
		m_filter_w_size = (int)filter_w_size;
		m_y_step = (int)y_step;
		m_x_step = (int)x_step;
		m_output_c_size = (int)output_c_size;
		m_output_h_size = ((m_input_h_size - m_filter_h_size + 1) + (m_y_step - 1)) / m_y_step;
		m_output_w_size = ((m_input_w_size - m_filter_w_size + 1) + (m_x_step - 1)) / m_x_step;
	}

	// 内部でROI設定を行うので、外部に見せるバッファポインタと別に設定する
	void  SetInputValueBuffer(NeuralNetBuffer<T, INDEX> buffer) {
		super::SetInputValueBuffer(buffer);
		m_filter_net->SetInputValueBuffer(buffer);
	}
	void  SetOutputValueBuffer(NeuralNetBuffer<T, INDEX> buffer) {
		super::SetOutputValueBuffer(buffer);
		m_filter_net->SetOutputValueBuffer(buffer);
	}
	void  SetInputErrorBuffer(NeuralNetBuffer<T, INDEX> buffer) {
		super::SetInputErrorBuffer(buffer);
		m_filter_net->SetInputErrorBuffer(buffer);
	}
	void  SetOutputErrorBuffer(NeuralNetBuffer<T, INDEX> buffer) {
		super::SetOutputErrorBuffer(buffer);
		m_filter_net->SetOutputErrorBuffer(buffer);
	}
	
//	const NeuralNetBuffer<T, INDEX>&  GetInputValueBuffer(void) const { return super::GetInputValueBuffer(); }
//	const NeuralNetBuffer<T, INDEX>&  GetOutputValueBuffer(void) const { return super::GetOutputValueBuffer(); }
//	const NeuralNetBuffer<T, INDEX>&  GetInputErrorBuffer(void) const { return super::GetInputErrorBuffer(); }
//	const NeuralNetBuffer<T, INDEX>&  GetOutputErrorBuffer(void) const { return super::GetOutputErrorBuffer(); }

	void  SetMuxSize(INDEX mux_size)
	{
		m_mux_size = mux_size;
		m_filter_net->SetMuxSize(mux_size);
	}

	void SetBatchSize(INDEX batch_size) {
		m_frame_size = batch_size * m_mux_size;
		m_filter_net->SetBatchSize(batch_size);
	}
	
	INDEX GetInputFrameSize(void) const { return m_frame_size; }
	INDEX GetInputNodeSize(void) const { return m_input_c_size * m_input_h_size * m_input_w_size; }
	INDEX GetOutputFrameSize(void) const { return m_frame_size; }
	INDEX GetOutputNodeSize(void) const { return m_output_c_size * m_output_h_size * m_output_w_size; }
	
	int   GetInputValueDataType(void) const { return BB_TYPE_BINARY; }
	int   GetInputErrorDataType(void) const { return BB_TYPE_BINARY; }
	int   GetOutputValueDataType(void) const { return BB_TYPE_BINARY; }
	int   GetOutputErrorDataType(void) const { return BB_TYPE_BINARY; }
	
protected:

	inline T* GetInputPtr(NeuralNetBuffer<T, INDEX>& buf, int c, int y, int x)
	{
		return buf.GetPtr<T>((c*m_input_h_size + y)*m_input_w_size + x);
	}

	inline T* GetOutputPtr(NeuralNetBuffer<T, INDEX>& buf, int c, int y, int x)
	{
		return buf.GetPtr<T>((c*m_output_h_size + y)*m_output_w_size + x);
	}

public:
	void Forward(void)
	{
		auto in_val = GetInputValueBuffer();
		auto out_val = GetOutputValueBuffer();

		in_val.SetDimensions({ m_input_w_size, m_input_h_size, m_input_c_size});
		out_val.SetDimensions({ m_output_w_size, m_output_h_size, m_output_c_size});
		
		INDEX in_y = 0;
		for (INDEX out_y = 0; out_y < m_output_h_size; ++out_y) {
			INDEX in_x = 0;
			for (INDEX out_x = 0; out_x < m_output_w_size; ++out_x) {
				in_val.ClearRoi();
				in_val.SetRoi({ in_x, in_y, 0}, { m_filter_w_size, m_filter_h_size, m_input_c_size });
				out_val.ClearRoi();
				out_val.SetRoi({ out_x, out_y, 0}, { 1, 1, m_output_c_size });

				m_filter_net->SetInputValueBuffer(in_val);
				m_filter_net->SetOutputValueBuffer(out_val);
				m_filter_net->Forward();

				in_x += m_x_step;
			}
			in_y += m_y_step;
		}

	//	in_val.ClearRoi();
	//	SetInputValueBuffer(in_val);
	//	out_val.ClearRoi();
	//	SetOutputValueBuffer(out_val);
	}
	
	void Backward(void)
	{
	}

	void Update(double learning_rate)
	{
	}

	bool Feedback(const std::vector<double>& loss)
	{
		if ( m_filter_net->Feedback(loss)) {
			Forward();	// 全体再計算
			return true;
		}
		
		return false;
	}


	// Serialize
public:
	template <class Archive>
	void save(Archive &archive, std::uint32_t const version) const
	{
		archive(cereal::make_nvp("NeuralNetLayer", *(NeuralNetLayer<T, INDEX>*)this));
	}

	template <class Archive>
	void load(Archive &archive, std::uint32_t const version)
	{
		archive(cereal::make_nvp("NeuralNetLayer", *(NeuralNetLayer<T, INDEX>*)this));
	}


	virtual void Save(cereal::JSONOutputArchive& archive) const
	{
		archive(cereal::make_nvp("NeuralNetBinaryFilter", *this));
		m_filter_net->Save(archive);
	}

	virtual void Load(cereal::JSONInputArchive& archive)
	{
		archive(cereal::make_nvp("NeuralNetBinaryFilter", *this));
		m_filter_net->Load(archive);
	}
};


}