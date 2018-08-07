#include <windows.h>
#pragma comment(lib, "winmm.lib")

#include <stdio.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <random>
#include <utility>
#include "mnist_read.h"
#include "LutNet.h"
#include "Lut6NetAvx2.h"
#include "ShuffleSet.h"
#include "LutNetBatch.h"
#include "Lut6NetBatchAvx2.h"
#include "Lut6NetBatchAvx2Byte.h"
#include "Lut6NetBatchAvx2Bit.h"


#define	LUT_SIZE				6

#define	USE_NET_LUT				0
#define	USE_NET_AVX				0

#define	USE_NET_BATCH0			0
#define	USE_NET_BATCH1			1


#define	INPUT_NUM				(28*28)
#define	OUTPUT_NUM				10

#define	EVA_RATE				4

#define	BATCH_SIZE				(8192)

#define UPDATE_FIX_TH_LOOP		1
#define UPDATE_RAND_LOOP		2

//#define UPDATE_GAIN				2.0
//#define UPDATE_GAIN				100.0
#define UPDATE_GAIN				10.0

//#define IMG_RAND_TH_MIN			(0   - 10)
//#define IMG_RAND_TH_MAX			(255 + 10)
#define IMG_RAND_TH_MIN			(0)
#define IMG_RAND_TH_MAX			(254)



//std::vector<int>	layer_num{ INPUT_NUM, 200, 50, OUTPUT_NUM };
//std::vector<int>	layer_num{ INPUT_NUM, 360, 60, OUTPUT_NUM };
//std::vector<int>	layer_num{ INPUT_NUM, 200, 100, 50, OUTPUT_NUM };
//std::vector<int>	layer_num{ INPUT_NUM, 300, 200, 50, OUTPUT_NUM };
//std::vector<int>	layer_num{ INPUT_NUM, 400, 300, 200, 50, OUTPUT_NUM };
//std::vector<int>	layer_num{ INPUT_NUM, 400, 600, 200, 50, OUTPUT_NUM };
std::vector<int>	layer_num{ INPUT_NUM, 600, 600, 300, 50, OUTPUT_NUM };

//std::vector<int>	layer_num{INPUT_NUM, 4096, 512, 128, 32, OUTPUT_NUM};


void update_net(BinaryNet& net, std::vector< std::vector<uint8_t> > image, std::vector<uint8_t> label, std::mt19937& mt);
float evaluate_net(BinaryNet& net, std::vector< std::vector<uint8_t> > image, std::vector<uint8_t> label);

void update_net(BinaryNetBatch& net, std::vector< std::vector<uint8_t> > image, std::vector<uint8_t> label, std::mt19937& mt);
float evaluate_net(BinaryNetBatch& net, std::vector< std::vector<uint8_t> > image, std::vector<uint8_t> label);


#include <omp.h>



void WriteRtl(std::ostream& os, BinaryNetData& bnd);


class WriteLog
{
protected:
	int				m_serial_num = 0;
	double			m_max_accuracy = 0;
	std::string		m_prefix;
	DWORD			m_start_time;

public:
	WriteLog(std::string prefix = "")
	{
		m_prefix = prefix;
		m_start_time = timeGetTime();
	}

	void Write(BinaryNetData& bnd, double accuracy)
	{
		char fname[64];
		DWORD tm = (timeGetTime() - m_start_time) / 1000;

		// ���x�X�V�Ȃ�ۑ�
		if (accuracy > m_max_accuracy) {
			// RTL�ۑ�
			m_max_accuracy = accuracy;
			WriteRtl(std::ofstream(m_prefix + "rtl_max.v"), bnd);

			// JSON�ۑ�
			std::ofstream ofsJson(m_prefix + "net_max.json");
			cereal::JSONOutputArchive o_archive(ofsJson);
			o_archive(bnd);
		}

		{
			// RTL�ۑ�
			sprintf_s<64>(fname, "%srtl_last.v", m_prefix.c_str());
			WriteRtl(std::ofstream(fname), bnd);

			// JSON�ۑ�
			sprintf_s<64>(fname, "%snet_last.json", m_prefix.c_str());
			std::ofstream ofsJson(fname);
			cereal::JSONOutputArchive o_archive(ofsJson);
			o_archive(bnd);
		}

		{
			// RTL�ۑ�
			sprintf_s<64>(fname, "%srtl_%04d.v", m_prefix.c_str(), m_serial_num);
			WriteRtl(std::ofstream(fname), bnd);

			// JSON�ۑ�
			sprintf_s<64>(fname, "%snet_%04d.json", m_prefix.c_str(), m_serial_num);
			std::ofstream ofsJson(fname);
			cereal::JSONOutputArchive o_archive(ofsJson);
			o_archive(bnd);
		}


		// ���O
		std::ofstream ofsLog(m_prefix + "log.txt", std::ios::app);

		std::stringstream ss_log;
		ss_log << tm << "[s] " << m_prefix << m_serial_num << " : " << accuracy << " (max : " << m_max_accuracy << ")";

		std::cout << ss_log.str() << std::endl;
		ofsLog    << ss_log.str() << std::endl;

		m_serial_num++;
	}
};



int main()
{
	omp_set_num_threads(6);

	std::mt19937	mt(1);

	// MNIST�f�[�^�ǂݍ���
	auto train_image = mnist_read_image("train-images-idx3-ubyte");
	auto train_label = mnist_read_labels("train-labels-idx1-ubyte");
	auto test_image = mnist_read_image("t10k-images-idx3-ubyte");
	auto test_label = mnist_read_labels("t10k-labels-idx1-ubyte");

#ifdef _DEBUG
	test_image.resize(10);
	test_label.resize(10);
#endif


	// �f�[�^�I��p
	std::vector<size_t> train_idx(train_image.size());
	for (size_t i = 0; i < train_image.size(); i++) {
		train_idx[i] = i;
	}
	std::uniform_int_distribution<int>	distribution(0, (int)train_image.size() - 1);
	int batch_size = BATCH_SIZE;
	auto batch_image = train_image;
	auto batch_label = train_label;
	batch_image.resize(batch_size);
	batch_label.resize(batch_size);
	
	// �l�b�g�\�z(�b���͂Q��ނ܂킵�āA�o�O���)
	LutNet<LUT_SIZE>		net_lut(layer_num);
	Lut6NetAvx2				net_avx(layer_num);
//	LutNetBatch<6>			net_batch_lut(layer_num);
//	Lut6NetBatchAvx2		net_batch0(layer_num);
//	Lut6NetBatchAvx2		net_batch1(layer_num);
	Lut6NetBatchAvx2Byte	net_batch0(layer_num);
	Lut6NetBatchAvx2Bit		net_batch1(layer_num);

	WriteLog	log_batch1("batch1_");

	// LUT�𗐐��ŏ�����
	std::uniform_int_distribution<int>	rand_0_1(0, 1);
	for (int layer = 1; layer < (int)layer_num.size(); layer++ ) {
		for (int node = 0; node < layer_num[layer]; node++) {
			for ( int i = 0; i < (1<<LUT_SIZE); i++) {
				bool val = rand_0_1(mt) == 1 ? true : false;
				net_lut.SetLutBit(layer, node, i, val);
				net_avx.SetLutBit(layer, node, i, val);
				net_batch0.SetLutBit(layer, node, i, val);
				net_batch1.SetLutBit(layer, node, i, val);
			}
		}
	}

	// �����_���ɐڑ�
	for (int layer = 1; layer < (int)layer_num.size(); layer++) {
#if 0
		// �e�[�u���쐬
		int input_num = layer_num[layer - 1];
		std::uniform_int_distribution<size_t>	rand_con(0, input_num - 1);
		std::vector<int>	idx(input_num);
		for (int i = 0; i < input_num; i++) {
			idx[i] = i;
		}

		for (int node = 0; node < layer_num[layer]; node++) {
			// �V���b�t��
			for (int i = 0; i < LUT_SIZE; i++) {
				std::swap(idx[i], idx[rand_con(mt)]);
			}

			// �ڑ�
			for (int i = 0; i < LUT_SIZE; i++) {
				net_lut.SetConnection(layer, node, i, idx[i]);
				net_avx.SetConnection(layer, node, i, idx[i]);
			}
		}
#else
		ShuffleSet	ss(layer_num[layer - 1], mt());
		for (int node = 0; node < layer_num[layer]; node++) {
			auto set = ss.GetSet(LUT_SIZE);
			for (int i = 0; i < LUT_SIZE; i++) {
				net_lut.SetConnection(layer, node, i, set[i]);
				net_avx.SetConnection(layer, node, i, set[i]);
				net_batch0.SetConnection(layer, node, i, set[i]);
				net_batch1.SetConnection(layer, node, i, set[i]);
			}
		}
#endif
	}

	
	// �����]��
#if USE_NET_BATCH0
	printf("batch0:%f\n", evaluate_net(net_batch0, test_image, test_label));
#endif

#if USE_NET_BATCH1
	auto batch1_accuracy = evaluate_net(net_batch1, test_image, test_label);
	log_batch1.Write(net_batch1.ExportData(), batch1_accuracy);
#endif

#if USE_NET_LUT
	printf("lut:%f\n", evaluate_net(net_lut, test_image, test_label));
#endif

#if USE_NET_AVX
	printf("avx:%f\n", evaluate_net(net_avx, test_image, test_label));
#endif


	std::mt19937	mt_batch0(2);
	std::mt19937	mt_batch1(2);
	std::mt19937	mt_lut(2);
	std::mt19937	mt_avx(2);

	double	max_rate = 0;
	for (int iteration = 0; iteration < 1000; iteration++) {
		// �w�K�f�[�^�I��
		for (int i = 0; i < batch_size; i++) {
			std::swap(train_idx[i], train_idx[distribution(mt)]);
		}
		for (int i = 0; i < batch_size; i++) {
			batch_image[i] = train_image[train_idx[i]];
			batch_label[i] = train_label[train_idx[i]];
		}

#if USE_NET_BATCH0
		DWORD tm_b0_s = timeGetTime();
		update_net(net_batch0, batch_image, batch_label, mt_batch0);
		DWORD tm_b0_e = timeGetTime();
		printf("batch0:%d[ms]\n", (int)(tm_b0_e - tm_b0_s));
#endif

#if USE_NET_BATCH1
		DWORD tm_b1_s = timeGetTime();
		update_net(net_batch1, batch_image, batch_label, mt_batch1);
		DWORD tm_b1_e = timeGetTime();
		printf("batch1:%d[ms]\n", (int)(tm_b1_e - tm_b1_s));
#endif

#if USE_NET_LUT
		DWORD tm0_s = timeGetTime();
		update_net(net_lut, batch_image, batch_label, mt_lut);
		DWORD tm0_e = timeGetTime();
		printf("lut:%d[ms]\n", (int)(tm0_e - tm0_s));
#endif

#if USE_NET_AVX
		DWORD tm1_s = timeGetTime();
		update_net(net_avx, batch_image, batch_label, mt_avx);
		DWORD tm1_e = timeGetTime();
		printf("avx:%d[ms]\n", (int)(tm1_e - tm1_s));
#endif

		// ����I�ɕ]�����ċL�^
		if ( (iteration % EVA_RATE) == (EVA_RATE - 1) ) {
#if USE_NET_BATCH0
			printf("batch0:%f\n", evaluate_net(net_batch0, test_image, test_label));
#endif

#if USE_NET_BATCH1
	//		printf("batch1:%f\n", evaluate_net(net_batch1, test_image, test_label));
			auto batch1_accuracy = evaluate_net(net_batch1, test_image, test_label);
			log_batch1.Write(net_batch1.ExportData(), batch1_accuracy); 
#endif

#if USE_NET_LUT
			printf("lut:%f\n", evaluate_net(net_lut, test_image, test_label));
#endif

#if USE_NET_AVX
			auto NetData = net_avx.ExportData();

			char rtl_name[64];
			sprintf_s<64>(rtl_name, "rtl_%04d.v", iteration);
			std::ofstream ofs(rtl_name);
			WriteRtl(ofs, NetData);

			WriteRtl(std::ofstream("rtl_last.v"), NetData);

			double rate = evaluate_net(net_avx, test_image, test_label);
			if (rate >= max_rate) {
				max_rate = rate;
				std::ofstream("net_max.json");
				cereal::JSONOutputArchive o_archive(ofs);
				o_archive(NetData);

				WriteRtl(std::ofstream("rtl_max.v"), NetData);
			}

			char fname[64];
			{
				sprintf_s<64>(fname, "net_%04d.json", iteration);
				std::ofstream ofs(fname);
				cereal::JSONOutputArchive o_archive(ofs);
				o_archive(NetData);
			}
			
			{
				std::ofstream ofs("net_last.json");
				cereal::JSONOutputArchive o_archive(ofs);
				o_archive(NetData);
			}

			printf("avx:%f (max:%f) : %s\n", rate, max_rate, fname);
#endif
		}
	}
	
	getchar();

	return 0;
}






// ���l�摜���痐���łQ�l�摜����
std::vector<bool> make_input_random(std::vector<uint8_t> img, unsigned int seed)
{
	std::vector<bool> input_vector(img.size());

	std::mt19937						mt(seed);
	std::uniform_int_distribution<int>	distribution(IMG_RAND_TH_MIN, IMG_RAND_TH_MAX);

	for (int i = 0; i < (int)img.size(); i++) {
		input_vector[i] = (img[i] > distribution(mt));
	}

	return input_vector;
}


// ���l�摜����Œ�臒l�łQ�l�摜����
std::vector<bool> make_input_th(std::vector<uint8_t> img, uint8_t th)
{
	std::vector<bool> input_vector(img.size());

	for (int i = 0; i < (int)img.size(); i++) {
		input_vector[i] = (img[i] > th);
	}

	return input_vector;
}



// �o�͂�]��
double calc_score(int exp, std::vector<bool> out_vec)
{
	double score = 0;
	for (int i = 0; i < (int)out_vec.size(); i++) {
		if (i == exp) {
			score += out_vec[i] ? +1.0 : -1.0;
		}
		else {
			score += out_vec[i] ? -0.1 : +0.1;
		}
	}
	return score;
}


// �l�b�g���X�V
void update_net(BinaryNet& net, std::vector< std::vector<uint8_t> > image, std::vector<uint8_t> label, std::mt19937& mt)
{
	std::uniform_int_distribution<int>	distribution(IMG_RAND_TH_MIN, IMG_RAND_TH_MAX);

	for (int layer = net.GetLayerNum() - 1; layer > 0; layer--) {
		int node_num = net.GetNodeNum(layer);
		for (int node = 0; node < node_num; node++) {
			double  score_val[64] = { 0 };
			int     score_n[64] = { 0 };

			for (int i = 0; i < (int)image.size(); i++) {
				// �Œ�臒l
				for (int j = 0; j < UPDATE_FIX_TH_LOOP; j++) {
					//		int th = 127; //  distribution(mt);
					net.SetInput(make_input_th(image[i], distribution(mt)));
					net.CalcForward();
					int  idx = net.GetInputLutIndex(layer, node);
					score_val[idx] += calc_score(label[i], net.GetOutput());
					score_n[idx]++;

					net.InvertLut(layer, node);
					net.CalcForward(layer);
					score_val[idx] -= calc_score(label[i], net.GetOutput());
					score_n[idx]++;
					net.InvertLut(layer, node);
				}

				// �����f�B�U
				for (int j = 0; j < UPDATE_RAND_LOOP; j++) {
					net.SetInput(make_input_random(image[i], mt()));
					net.CalcForward();
					int  idx = net.GetInputLutIndex(layer, node);
					score_val[idx] += calc_score(label[i], net.GetOutput());
					score_n[idx]++;

					net.InvertLut(layer, node);
					net.CalcForward(layer);
					score_val[idx] -= calc_score(label[i], net.GetOutput());
					score_n[idx]++;
					net.InvertLut(layer, node);
				}
			}

			// LUT�X�V
			std::uniform_real_distribution<double> score_th(-1.0, 0.0);
			for (int i = 0; i < 64; i++) {
				double score = score_val[i] / (double)score_n[i];
	//			if (score * UPDATE_GAIN < score_th(mt)) {
				if (score < 0.0) {
					net.SetLutBit(layer, node, i, !net.GetLutBit(layer, node, i));
				}
			}
		}
	}
}


#if 0
// �l�b�g���X�V(�o�b�`��)[�P�̂ƔłƐ��x��v]
void update_net(BinaryNetBatch& net, std::vector< std::vector<uint8_t> > image, std::vector<uint8_t> label, std::mt19937& mt)
{
	std::uniform_int_distribution<int>	distribution(IMG_RAND_TH_MIN, IMG_RAND_TH_MAX);

	int image_size = (int)image.size();
	int batch_size = image_size * (UPDATE_FIX_TH_LOOP + UPDATE_RAND_LOOP);
	net.SetBatchSize(batch_size);

	for (int layer = net.GetLayerNum() - 1; layer > 0; layer--) {
		int node_num = net.GetNodeNum(layer);
		for (int node = 0; node < node_num; node++) {
			double  score_val[64] = { 0 };
			int     score_n[64] = { 0 };
			
			int frame = 0;
			for (int i = 0; i < image_size; i++) {
				for (int j = 0; j < UPDATE_FIX_TH_LOOP; j++) {
					net.SetInput(frame++, make_input_th(image[i], distribution(mt)));
				}
				for (int j = 0; j < UPDATE_RAND_LOOP; j++) {
					net.SetInput(frame++, make_input_random(image[i], mt()));
				}
			}

			// �v�Z���s
			net.CalcForward();

			// ���ʕۑ�(��U�Č����m�ۂ̂��߂Ɍ�ŏW�v)
			std::vector< std::vector<bool> > vec_output(batch_size);
			for (int i = 0; i < batch_size; i++) {
				vec_output[i] = net.GetOutput(i);
			}

			// ���]
			net.InvertLut(layer, node);
			net.CalcForward(layer);

			// �W�v
			frame = 0;
			for (int i = 0; i < image_size; i++) {
				for (int j = 0; j < UPDATE_FIX_TH_LOOP + UPDATE_RAND_LOOP; j++) {
					int  idx = net.GetInputLutIndex(frame, layer, node);
					score_val[idx] += calc_score(label[i], vec_output[frame]);
					score_n[idx]++;
					score_val[idx] -= calc_score(label[i], net.GetOutput(frame));
					score_n[idx]++;
					frame++;
				}
			}

			// ���ɖ߂�
			net.InvertLut(layer, node);

			// LUT�X�V
			std::uniform_real_distribution<double> score_th(-1.0, 0.0);
			for (int i = 0; i < 64; i++) {
				double score = score_val[i] / (double)score_n[i];
				//			if (score * UPDATE_GAIN < score_th(mt)) {
				if (score < 0.0) {
					net.SetLutBit(layer, node, i, !net.GetLutBit(layer, node, i));
				}
			}
		}
	}
}

#else

// �l�b�g���X�V(�o�b�`��)
void update_net(BinaryNetBatch& net, std::vector< std::vector<uint8_t> > image, std::vector<uint8_t> label, std::mt19937& mt)
{
	std::uniform_int_distribution<int>	distribution(IMG_RAND_TH_MIN, IMG_RAND_TH_MAX);

	int image_size = (int)image.size();
	int batch_size = image_size * (UPDATE_FIX_TH_LOOP + UPDATE_RAND_LOOP);
	net.SetBatchSize(batch_size);

	{
		int frame = 0;
		for (int i = 0; i < image_size; i++) {
			for (int j = 0; j < UPDATE_FIX_TH_LOOP; j++) {
				net.SetInput(frame++, make_input_th(image[i], distribution(mt)));
			}
			for (int j = 0; j < UPDATE_RAND_LOOP; j++) {
				net.SetInput(frame++, make_input_random(image[i], mt()));
			}
		}
	}

	net.CalcForward();

	for (int layer = net.GetLayerNum() - 1; layer > 0; layer--) {
		int node_num = net.GetNodeNum(layer);
		for (int node = 0; node < node_num; node++) {
			double  score_val[64] = { 0 };
			int     score_n[64] = { 0 };

			// �v�Z���s
	//		net.CalcForward();

			// �W�v
			int frame = 0;
			for (int i = 0; i < image_size; i++) {
				for (int j = 0; j < UPDATE_FIX_TH_LOOP + UPDATE_RAND_LOOP; j++) {
					int  idx = net.GetInputLutIndex(frame, layer, node);
					score_val[idx] += calc_score(label[i], net.GetOutput(frame));
					score_n[idx]++;
					frame++;
				}
			}

			// ���ʕۑ�(��U�Č����m�ۂ̂��߂Ɍ�ŏW�v)
	//		std::vector< std::vector<bool> > vec_output(batch_size);
	//		for (int i = 0; i < batch_size; i++) {
	//			vec_output[i] = net.GetOutput(i);
	//		}

			// ���]
			net.InvertLut(layer, node);
			net.CalcForward(layer);

			// �W�v
			frame = 0;
			for (int i = 0; i < image_size; i++) {
				for (int j = 0; j < UPDATE_FIX_TH_LOOP + UPDATE_RAND_LOOP; j++) {
					int  idx = net.GetInputLutIndex(frame, layer, node);
					score_val[idx] -= calc_score(label[i], net.GetOutput(frame));
					score_n[idx]++;
					frame++;
				}
			}

			// ���ɖ߂�
			net.InvertLut(layer, node);
			
			// LUT�X�V
			std::uniform_real_distribution<double> score_th(-1.0, 0.0);
			for (int i = 0; i < 64; i++) {
				double score = score_val[i] / (double)score_n[i];
				//			if (score * UPDATE_GAIN < score_th(mt)) {
				if ( score < 0.0 ) {
					net.SetLutBit(layer, node, i, !net.GetLutBit(layer, node, i));
				}
			}

			// �����������ȍ~���Čv�Z
			net.CalcForwardNode(layer, node);
			net.CalcForward(layer);
		}
	}
}

#endif



// �l�b�g��]��
float evaluate_net(BinaryNet& net, std::vector< std::vector<uint8_t> > image, std::vector<uint8_t> label)
{
	int		n = 0;
	int		ok = 0;

	std::mt19937	mt(1);
	std::uniform_int_distribution<int>	distribution(IMG_RAND_TH_MIN, IMG_RAND_TH_MAX);

	int out_layer = net.GetLayerNum() - 1;
	auto label_it = label.begin();
	for (auto& img : image) {
		std::vector<int> count(10, 0);
		for (int i = 0; i < 32; i++) {
			auto in_vec = make_input_random(img, mt());
			net.SetInput(in_vec);
			net.CalcForward();

			for (int j = 0; j < 10; j++) {
				count[j] += net.GetValue(out_layer, j) ? 1 : 0;
			}
			

			net.SetInput(make_input_th(img, distribution(mt)));
			net.CalcForward();
			for (int j = 0; j < 10; j++) {
				count[j] += net.GetValue(out_layer, j) ? 1 : 0;
			}
		}

		auto	max_it = std::max_element(count.begin(), count.end());
		uint8_t max_idx = (uint8_t)std::distance(count.begin(), max_it);
		uint8_t label = *label_it++;
		if (max_idx == label) {
			ok++;
		}
		n++;
	}

	return (float)ok / (float)n;
}


float evaluate_net(BinaryNetBatch& net, std::vector< std::vector<uint8_t> > image, std::vector<uint8_t> label)
{

	std::mt19937	mt(1);
	std::uniform_int_distribution<int>	distribution(IMG_RAND_TH_MIN, IMG_RAND_TH_MAX);

	int image_size = (int)image.size();

	net.SetBatchSize(image_size * 64);

	int out_layer = net.GetLayerNum() - 1;
	int	frame = 0;

	// �f�[�^�ݒ�
	for (int img_idx = 0; img_idx < image_size; img_idx++) {
		for (int i = 0; i < 32; i++) {
			net.SetInput(frame++, make_input_random(image[img_idx], mt()));
			net.SetInput(frame++, make_input_th(image[img_idx], distribution(mt)));
		}
	}

	// �v�Z
	net.CalcForward();

	// �]��
	int		n = 0;
	int		ok = 0;
	frame = 0;
	for (int img_idx = 0; img_idx < image_size; img_idx++) {
		std::vector<int> count(10, 0);
		for (int i = 0; i < 64; i++) {
			for (int j = 0; j < 10; j++) {
				count[j] += net.GetValue(frame, out_layer, j) ? 1 : 0;
			}
			frame++;
		}

		auto	max_it = std::max_element(count.begin(), count.end());
		uint8_t max_idx = (uint8_t)std::distance(count.begin(), max_it);
		if (max_idx == label[img_idx]) {
			ok++;
		}
		n++;
	}
	
	return (float)ok / (float)n;
}

