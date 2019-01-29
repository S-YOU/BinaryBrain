// --------------------------------------------------------------------------
//  Binary Brain  -- binary neural net framework
//
//                                Copyright (C) 2018-2019 by Ryuji Fuchikami
//                                https://github.com/ryuz
//                                ryuji.fuchikami@nifty.com
// --------------------------------------------------------------------------


#pragma once


#ifdef BB_WITH_CUDA
#include "cuda_runtime.h"
#include "bbcu/bbcu_util.h"
#endif

#include "bb/DataType.h"
#include "bb/Utility.h"
#include "bb/CudaUtility.h"


namespace bb {

#define BB_MEMORY_MODE_WRITE		0x01
#define BB_MEMORY_MODE_READ			0x02
#define BB_MEMORY_MODE_NEW			0x04
#define BB_MEMORY_MODE_RW			(BB_MEMORY_MODE_WRITE | BB_MEMORY_MODE_READ)
#define BB_MEMORY_MODE_NW			(BB_MEMORY_MODE_NEW | BB_MEMORY_MODE_WRITE)
#define BB_MEMORY_MODE_NRW			(BB_MEMORY_MODE_NEW | BB_MEMORY_MODE_RW)


class Memory
{
protected:
	size_t	m_size = 0;
	void*	m_addr = nullptr;
    int     m_refcnt = 0;

#ifdef BB_WITH_CUDA
	int		m_device;
	void*	m_devAddr = nullptr;
	bool	m_hostModified = false;
	bool	m_devModified = false;
	int		m_devRefcnt = 0;
#endif

public:
	/**
     * @brief  �R���X�g���N�^
     * @detail �R���X�g���N�^
     * @param size �m�ۂ��郁�����T�C�Y(�o�C�g�P��)
	 * @param device ���p����GPU�f�o�C�X
	 *           0�ȏ�  ���݂̑I�𒆂�GPU
	 *           -1     ���݂̑I�𒆂�GPU
	 *           -2     GPU�͗��p���Ȃ�
     * @return �Ȃ�
     */
	explicit Memory(size_t size, int device=BB_DEVICE_CURRENT_GPU)
	{
		// �T�C�Y�ۑ�
		m_size = size;

#ifdef BB_WITH_CUDA
		// �f�o�C�X�ݒ�
		int dev_count = 0;
		auto status = cudaGetDeviceCount(&dev_count);
		if (status != cudaSuccess) {
			dev_count = 0;
		}

		// ���݂̃f�o�C�X���擾
		if ( device == BB_DEVICE_CURRENT_GPU && dev_count > 0 ) {
			BB_CUDA_SAFE_CALL(cudaGetDevice(&device));
		}

		// GPU�����݂���ꍇ
		if ( device >= 0 && device < dev_count ) {
			m_device = device;
		}
		else {
			// �w��f�o�C�X�����݂��Ȃ��ꍇ��CPU
			m_device = BB_DEVICE_CPU;
			m_addr = aligned_memory_alloc(m_size, 32);
		}
#else
		// �������m��
		m_addr = aligned_memory_alloc(m_size, 32);
#endif
	}

	/**
     * @brief  �f�X�g���N�^
     * @detail �f�X�g���N�^
     */
	~Memory()
	{
        BB_DEBUG_ASSERT(m_refcnt != 0);

#ifdef BB_WITH_CUDA
        BB_DEBUG_ASSERT(m_devRefcnt != 0);

		if ( m_device >= 0 ) {
			CudaDevicePush dev_push(m_device);

			// �������J��
			if (m_addr != nullptr) {
				BB_CUDA_SAFE_CALL(cudaFreeHost(m_addr));
			}
			if (m_devAddr != nullptr) {
				BB_CUDA_SAFE_CALL(cudaFree(m_devAddr));
			}
		}
		else {
			// �������J��
			if (m_addr != nullptr) {
				aligned_memory_free(m_addr);
			}
		}
#else
		// �������J��
		if (m_addr != nullptr) {
			aligned_memory_free(m_addr);
		}
#endif
	}

	/**
     * @brief  �f�o�C�X�����p�\���₢���킹��
     * @detail �f�o�C�X�����p�\���₢���킹��
     * @return �f�o�C�X�����p�\�Ȃ�true
     */
	bool IsDeviceAvailable(void)
	{
#ifdef BB_WITH_CUDA
		return (m_device >= 0);
#else
		return false;
#endif
	}
	

	/**
     * @brief  ���������e�̔j��
     * @detail ���������e��j������
     */	void Dispose(void)
	{
#ifdef BB_WITH_CUDA
		// �X�V�̔j��
		m_hostModified = false;
		m_devModified = false;
#endif
	}

	/**
     * @brief  �������T�C�Y�̎擾
     * @detail �������T�C�Y�̎擾
     * @return �������T�C�Y(�o�C�g�P��)
     */
	INDEX GetSize(void)
	{
		return m_size;
	}

	/**
     * @brief  �|�C���^�̎擾
     * @detail �A�N�Z�X�p�Ɋm�ۂ����z�X�g���̃������|�C���^�̎擾
     * @param  mode �ȉ��̃t���O�̑g�ݍ��킹 
     *           BB_MEM_WRITE   �������݂��s��
     *           BB_MEM_READ    �ǂݍ��݂��s��
     *           BB_MEM_NEW		�V�K���p(�ȑO�̓��e�̔j��)
     * @return �A�N�Z�X�p�Ɋm�ۂ����z�X�g���̃������|�C���^
     */
	void* Lock(int mode=BB_MEMORY_MODE_RW)
	{
#ifdef BB_WITH_CUDA
		if ( m_device >= 0 ) {
			// �V�K�ł���Ήߋ��̍X�V���͔j��
			if (mode & BB_MEMORY_MODE_NEW) {
				m_hostModified = false;
				m_devModified = false;
			}

			if (m_addr == nullptr) {
				// �z�X�g�����������m�ۂȂ炱���Ŋm��
				CudaDevicePush dev_push(m_device);
				BB_CUDA_SAFE_CALL(cudaMallocHost(&m_addr, m_size));
			}

			if ( m_devModified ) {
				// �f�o�C�X�����������ŐV�Ȃ�R�s�[�擾
				CudaDevicePush dev_push(m_device);
				BB_CUDA_SAFE_CALL(cudaMemcpy(m_addr, m_devAddr, m_size, cudaMemcpyDeviceToHost));
				m_devModified =false;
			}

			// �������݂��s���Ȃ�C���t���O�Z�b�g
			if (mode & BB_MEMORY_MODE_WRITE) {
				m_hostModified = true;
			}
		}
#endif
        m_refcnt++;
		return m_addr;
	}


  	void Unlock(void)
	{
        m_refcnt--;
    }


	/**
     * @brief  �f�o�C�X���|�C���^�̎擾
     * @detail �A�N�Z�X�p�Ɋm�ۂ����f�o�C�X���̃������A�h���X�̎擾
     * @param  mode �ȉ��̃t���O�̑g�ݍ��킹 
     *           BB_MEM_WRITE   �������݂��s��
     *           BB_MEM_READ    �ǂݍ��݂��s��
     *           BB_MEM_NEW		�V�K���p(�ȑO�̓��e�̔j��)
     * @return �A�N�Z�X�p�Ɋm�ۂ����f�o�C�X���̃������A�h���X
     */
	void* LockDevice(int mode=BB_MEMORY_MODE_RW)
	{
	#ifdef BB_WITH_CUDA
		if ( m_device >= 0 ) {
			// �V�K�ł���Ήߋ��̍X�V���͔j��
			if (mode & BB_MEMORY_MODE_NEW) {
				m_hostModified = false;
				m_devModified = false;
			}

			if (m_devAddr == nullptr) {
				// �f�o�C�X�����������m�ۂȂ炱���Ŋm��
				CudaDevicePush dev_push(m_device);
				BB_CUDA_SAFE_CALL(cudaMalloc(&m_devAddr, m_size));
			}

			if (m_hostModified) {
				// �z�X�g�����������ŐV�Ȃ�R�s�[�擾
				CudaDevicePush dev_push(m_device);
				BB_CUDA_SAFE_CALL(cudaMemcpy(m_devAddr, m_addr, m_size, cudaMemcpyHostToDevice));
				m_hostModified =false;
			}

			// �������݂��s���Ȃ�C���t���O�Z�b�g
			if (mode & BB_MEMORY_MODE_WRITE) {
				m_devModified = true;
			}

            m_refcnt++;
			return m_devAddr;
		}
#endif

		return nullptr;
	}

    void UnlockDevice(void)
    {
        m_refcnt--;
    }
};


}


// end of file
