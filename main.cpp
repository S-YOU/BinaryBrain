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


#define	LUT_SIZE				6

#define	INPUT_NUM				(28*28)
#define	OUTPUT_NUM				10

#define	BATCH_SIZE				10000

#define UPDATE_FIX_TH_LOOP		0
#define UPDATE_RAND_LOOP		1

//#define UPDATE_GAIN				2.0
#define UPDATE_GAIN				100.0

//#define IMG_RAND_TH_MIN			(0   - 10)
//#define IMG_RAND_TH_MAX			(255 + 10)
#define IMG_RAND_TH_MIN			(0)
#define IMG_RAND_TH_MAX			(254)



//std::vector<int>	layer_num{ INPUT_NUM, 200, 50, OUTPUT_NUM };
//std::vector<int>	layer_num{ INPUT_NUM, 360, 60, OUTPUT_NUM };
//std::vector<int>	layer_num{ INPUT_NUM, 200, 100, 50, OUTPUT_NUM };
//std::vector<int>	layer_num{ INPUT_NUM, 300, 200, 50, OUTPUT_NUM };
std::vector<int>	layer_num{ INPUT_NUM, 400, 300, 200, 50, OUTPUT_NUM };

//std::vector<int>	layer_num{INPUT_NUM, 4096, 512, 128, 32, OUTPUT_NUM};


void update_net(BinaryNet& net, std::vector< std::vector<uint8_t> > image, std::vector<uint8_t> label, std::mt19937& mt);
float evaluate_net(BinaryNet& net, std::vector< std::vector<uint8_t> > image, std::vector<uint8_t> label);



#include <omp.h>

#define		USE_LUT		0
#define		USE_AVX		1


void WriteRtl(std::ostream& os, BinaryNetData& bnd);


int main()
{
	omp_set_num_threads(4);

	std::mt19937	mt(1);

	// MNIST�f�[�^�ǂݍ���
	auto train_image = mnist_read_image("train-images-idx3-ubyte");
	auto train_label = mnist_read_labels("train-labels-idx1-ubyte");
	auto test_image = mnist_read_image("t10k-images-idx3-ubyte");
	auto test_label = mnist_read_labels("t10k-labels-idx1-ubyte");

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
	LutNet<LUT_SIZE>	net_lut(layer_num);
	Lut6NetAvx2			net_avx(layer_num);

	// LUT�𗐐��ŏ�����
	std::uniform_int_distribution<int>	rand_0_1(0, 1);
	for (int layer = 1; layer < (int)layer_num.size(); layer++ ) {
		for (int node = 0; node < layer_num[layer]; node++) {
			for ( int i = 0; i < (1<<LUT_SIZE); i++) {
				bool val = rand_0_1(mt) == 1 ? true : false;
				net_lut.SetLutBit(layer, node, i, val);
				net_avx.SetLutBit(layer, node, i, val);
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
			}
		}
#endif
	}

	{
		std::ofstream ofs("rtl.v");
		WriteRtl(ofs, net_avx.ExportData());
	}
	
	// �����]��
#if USE_LUT
	printf("lut:%f\n", evaluate_net(net_lut, test_image, test_label));
#endif

#if USE_AVX
	printf("avx:%f\n", evaluate_net(net_avx, test_image, test_label));
#endif

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
		
#if USE_LUT
		DWORD tm0_s = timeGetTime();
		update_net(net_lut, batch_image, batch_label, mt_lut);
		DWORD tm0_e = timeGetTime();
	//	printf("lut:%d[ms]\n", (int)(tm0_e - tm0_s));
#endif

#if USE_AVX
		DWORD tm1_s = timeGetTime();
		update_net(net_avx, batch_image, batch_label, mt_avx);
		DWORD tm1_e = timeGetTime();
	//	printf("avx:%d[ms]\n", (int)(tm1_e - tm1_s));
#endif

		if (iteration % 1 == 0) {
#if USE_LUT
			printf("lut:%f\n", evaluate_net(net_lut, test_image, test_label));
#endif
#if USE_AVX
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
					int th = distribution(mt);
					net.SetInput(make_input_th(image[i], th));
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




// �l�b�g��]��
float evaluate_net(BinaryNet& net, std::vector< std::vector<uint8_t> > image, std::vector<uint8_t> label)
{
	int		n = 0;
	int		ok = 0;

	std::mt19937	mt(1);

	int out_layer = net.GetLayerNum() - 1;
	auto label_it = label.begin();
	for (auto& img : image) {
		std::vector<int> count(10, 0);
		for (int i = 0; i < 16; i++) {
			auto in_vec = make_input_random(img, mt());
			net.SetInput(in_vec);
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
