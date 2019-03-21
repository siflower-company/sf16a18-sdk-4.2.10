#ifndef _CRYPTO_H
#define _CRYPTO_H

#define	CRYPTO_BRDMA_START				0x0000
#define	CRYPTO_BRDMA_ADMA_CFG				0x0004
#define	CRYPTO_BRDMA_ADMA_ADDR			0x0008
#define	CRYPTO_BRDMA_SDMA_CTRL0			0x000C
#define	CRYPTO_BRDMA_SDMA_CTRL1			0x0010
#define	CRYPTO_BRDMA_SDMA_ADDR0			0x0014
#define	CRYPTO_BRDMA_SDMA_ADDR1			0x0018
#define	CRYPTO_BRDMA_INT_ACK				0x001C
#define	CRYPTO_BRDMA_STATE				0x0020
#define	CRYPTO_BRDMA_STATUS				0x0024
#define	CRYPTO_BRDMA_INT_MASK				0x0028
#define	CRYPTO_BRDMA_INT_STAT				0x002C
#define	CRYPTO_BTDMA_START				0x0100
#define	CRYPTO_BTDMA_ADMA_CFG				0x0104
#define	CRYPTO_BTDMA_ADMA_ADDR			0x0108
#define	CRYPTO_BTDMA_SDMA_CTRL0			0x010C
#define	CRYPTO_BTDMA_SDMA_CTRL1			0x0110
#define	CRYPTO_BTDMA_SDMA_ADDR0			0x0114
#define	CRYPTO_BTDMA_SDMA_ADDR1			0x0118
#define	CRYPTO_BTDMA_INT_ACK				0x011C
#define	CRYPTO_BTDMA_STATE				0x0120
#define	CRYPTO_BTDMA_STATUS				0x0124
#define	CRYPTO_BTDMA_INT_MASK				0x0128
#define	CRYPTO_BTDMA_INT_STAT				0x012C
#define	CRYPTO_HRDMA_START				0x0200
#define	CRYPTO_HRDMA_ADMA_CFG				0x0204
#define	CRYPTO_HRDMA_ADMA_ADDR			0x0208
#define	CRYPTO_HRDMA_SDMA_CTRL0			0x020C
#define	CRYPTO_HRDMA_SDMA_CTRL1			0x0210
#define	CRYPTO_HRDMA_SDMA_ADDR0			0x0214
#define	CRYPTO_HRDMA_SDMA_ADDR1			0x0218
#define	CRYPTO_HRDMA_INT_ACK				0x021C
#define	CRYPTO_HRDMA_STATE				0x0220
#define	CRYPTO_HRDMA_STATUS				0x0224
#define	CRYPTO_HRDMA_INT_MASK				0x0228
#define	CRYPTO_HRDMA_INT_STAT				0x022C
#define	CRYPTO_PKADMA_START				0x0300
#define	CRYPTO_PKADMA_ADMA_CFG			0x0304
#define	CRYPTO_PKADMA_ADMA_ADDR			0x0308
#define	CRYPTO_PKADMA_SDMA_CTRL0			0x030C
#define	CRYPTO_PKADMA_SDMA_CTRL1			0x0310
#define	CRYPTO_PKADMA_SDMA_ADDR0			0x0314
#define	CRYPTO_PKADMA_SDMA_ADDR1			0x0318
#define	CRYPTO_PKADMA_INT_ACK				0x031C
#define	CRYPTO_PKADMA_STATE				0x0320
#define	CRYPTO_PKADMA_STATUS				0x0324
#define	CRYPTO_PKADMA_INT_MASK			0x0328
#define	CRYPTO_PKADMA_INT_STAT			0x032C
#define	CRYPTO_BLKC_CONTROL				0x0400
#define	CRYPTO_BLKC_FIFO_MODE_EN			0x0404
#define	CRYPTO_BLKC_SWAP					0x0408
#define	CRYPTO_BLKC_STATUS				0x040C
#define	CRYPTO_AES_CNTDATA1				0x0410
#define	CRYPTO_AES_CNTDATA2				0x0414
#define	CRYPTO_AES_CNTDATA3				0x0418
#define	CRYPTO_AES_CNTDATA4				0x041C
#define	CRYPTO_AES_KEYDATA(n)			(0x0420 + n *0x4)
#define	CRYPTO_AES_IVDATA(n)			(0x0440 + n * 0x4)
#define	CRYPTO_AES_INDATA(n)			(0x0450 + n * 0x4)
#define	CRYPTO_AES_OUTDATA(n)			(0x0460 + n * 0x4)
#define	CRYPTO_AES_OUTDATA2				0x0464
#define	CRYPTO_AES_OUTDATA3				0x0468
#define	CRYPTO_AES_OUTDATA4				0x046C
#define	CRYPTO_TDES_CNTDATA1				0x0470
#define	CRYPTO_TDES_CNTDATA2				0x0474
#define	CRYPTO_TDES_KEYDATA(n)		(0x0478 + n *0x4)
#define	CRYPTO_TDES_IVDATA1				0x0490
#define	CRYPTO_TDES_IVDATA2				0x0494
#define	CRYPTO_TDES_INDATA(n)			(0x0498 + n * 0x4)
#define	CRYPTO_TDES_OUTDATA(n)			(0x04A0 + n * 0x4)
#define	CRYPTO_RC4_KEYDATA(n)			(0x04A8 + n * 0x4)
#define	CRYPTO_RC4_INDATA				0x04C8
#define	CRYPTO_RC4_OUTDATA				0x04CC
#define	CRYPTO_BLKC_INT_MASK			0x04D0
#define	CRYPTO_BLKC_INT_STAT			0x04D4
#define	CRYPTO_BLKC_STREAM_IDAT			0x04D8
#define	CRYPTO_BLKC_STREAM_ODAT			0x04DC
#define	CRYPTO_AES_CUSTOM_SBOX			0x0500
#define	CRYPTO_TDES_CUSTOM_SBOX			0x0600
#define	CRYPTO_RC4_CUSTOM_SBOX			0x0700
#define	CRYPTO_HASH_CONTROL				0x0800
#define	CRYPTO_HASH_FIFO_MODE_EN		0x0804
#define	CRYPTO_HASH_SWAP				0x0808
#define	CRYPTO_HASH_STATUS				0x080C
#define	CRYPTO_HASH_MSG_SIZE_LOW		0x0810
#define	CRYPTO_HASH_MSG_SIZE_HIGH		0x0814
#define CRYPTO_HASH_DATA_IN(n)          (0x0818 + n * 0x4)
#define	CRYPTO_HASH_CV1					0x0838
#define	CRYPTO_HASH_CV2					0x083C
#define	CRYPTO_HASH_CV3					0x0840
#define	CRYPTO_HASH_CV4					0x0844
#define	CRYPTO_HASH_CV5					0x0848
#define	CRYPTO_HASH_CV6					0x084C
#define	CRYPTO_HASH_CV7					0x0850
#define	CRYPTO_HASH_CV8					0x0854
#define	CRYPTO_HASH_RESULT(n)			(0x0858 + n * 0x4)
#define	CRYPTO_HASH_HMAC_KEY(n)			(0x0878 + n * 0x4)
#define	CRYPTO_HASH_INT_MASK			0x08B8
#define	CRYPTO_HASH_INT_STAT			0x08BC
#define	CRYPTO_PRNG_CONTROL				0x0900
#define	CRYPTO_PRNG_SEED				0x0904
#define	CRYPTO_PRNG_RNDNUM1				0x0908
#define	CRYPTO_PRNG_RNDNUM2				0x090C
#define	CRYPTO_PRNG_RNDNUM3				0x0910
#define	CRYPTO_PRNG_PNDNUM4				0x0914
#define	CRYPTO_PRNG_PNDNUM5				0x0918
#define	CRYPTO_PRNG_PNDNUM6				0x091C
#define	CRYPTO_PRNG_RNDNUM7				0x0920
#define	CRYPTO_PRNG_RNDNUM8				0x0924
#define	CRYPTO_PKA_CONTROL				0x0A00
#define	CRYPTO_PKA_FIFO_MODE_EN			0x0A04
#define	CRYPTO_PKA_SWAP					0x0A08
#define	CRYPTO_PKA_STATUS					0x0A0C
#define	CRYPTO_PKA_STATE					0x0A10
#define	CRYPTO_PKA_Q_IN1					0x0A14
#define	CRYPTO_PKA_Q_IN2					0x0A18
#define	CRYPTO_PKA_INT_MASK				0x0A1C
#define	CRYPTO_PKA_INT_STAT				0x0A20
#define	CRYPTO_PKA_DE_BUF					0x1000
#define	CRYPTO_PKA_P_BUF					0x1400
#define	CRYPTO_PKA_R_BUF					0x1800
#define	CRYPTO_PKA_MC_BUF					0x1C00
#define	CRYPTO_CG_CFG						0x2000

struct cryption_reg {
	u32 reg;
	u32 value;
};

struct cryption_write_reg {
	u32 reg;
	u32 write_value;
	u32 cmp_value;
};


typedef enum cryption_type_t{
	DECRYPTO,
	ENCRYPTO
}cryption_type;

typedef enum cryption_blkc_key_t{
	CUSTOM_KEY = 0x0,
	DEVICE_ID = 0x1,
	SECURITY_KEY = 0x3
}cryption_blkc_key;

typedef enum cryption_sbox_type_t{
	SBOX_DEFAULT,
	SBOX_CUSTOM
}cryption_sbox_type;

typedef enum cryption_stream_size_t{
	BITE_8 = 0x0,
	BITE_16 = 0x1,
	BITE_32 = 0x2,
	BITE_64 = 0x4,
	BITE_128 = 0x8
}cryption_stream_size;

typedef enum blkc_mode_t{
	AES = 0,
	DES,
	TDES,
	RC4	
}blkc_cryption_mode;

typedef enum blkc_chain_mode_t{
	ECB = 0x1,
	CBC = 0x2,
	CFB = 0x4,
	OFB = 0x8,
	CTR = 0x10
}blkc_cryption_chain_mode;

typedef enum hash_mode_t{
	MD5,
	SHA_1,
	SHA_224,
	SHA_256,
	MD5_HMAC,
	SHA_1_HMAC,
	SHA_224_HMAC,
	SHA_256_HMAC
}hash_mode;

typedef enum hash_vector_t{
	DEFAULT_VECTOR,
	CUSTOM_VECTOR
}hash_vector;

typedef enum hash_source_t{
	INDEP_SOURCE,
	CIPHER_INPUT,
	CIPHER_OUTPUT
}hash_source;

typedef enum pka_precision_t{
	STANDARD,
	DOUBLE,
	TRIPLE,
	QUADRUPLE
}pka_precision;

typedef enum pka_used_func_t{
	MUL,
	EXP
}pka_used_func;

typedef enum pka_mul_func_t{
	ABYB,
	ABYONE
}pka_mul_func;

typedef enum dma_type_t{
	BRDMA,
	BTDMA,
	HRDMA,
	PKADMA
}dma_type;

typedef enum dma_page_boundary_t{
	PAGE_4K,
	PAGE_8K,
	PAGE_16K,
	PAGE_32K,
 	PAGE_64K,
	PAGE_128k,
	PAGE_256K,
	PAGE_512K
}dma_page_boundary;

typedef enum dma_interrupt_mode_t{
	NO_INT,		// no interrupt
	CH0_INT,    // only channel 0 generate interrupt
	CH1_INT,	// only channel 1 generate interrupt
	BOTH_INT	// both channel 0 and channel 1 generate interrupt
}dma_interrupt_mode;

typedef enum clk_type_t{
	BLKC_CLK = 1,
	HASH_CLK = 2,
	PKA_CLK  = 4
}clk_type;

typedef enum en_state_t{
	DISABLE,
	ENABLE
}en_state;

typedef enum dma_control_mode_t{
	SDMA,
	ADMA
}dma_control_mode;

typedef enum sdma_mode_t{
	ONE_CH,
	TWO_CH
}sdma_mode;

typedef enum cryption_blkc_key_type_t{
	AES_KEY,
	DES_KEY,
	RC4_KEY
}cryption_blkc_key_type;

typedef enum cryption_irq_t{
	BRDMA_IRQ,
	BTDMA_IRQ,
	HRDMA_IRQ,
	PKADMA_IRQ
}cryption_irq;

#define SFAX8_SHAM_MD5				(0 << 3)
#define SFAX8_SHAM_SHA1				(1 << 3)
#define SFAX8_SHAM_SHA224			(2 << 3)
#define SFAX8_SHAM_SHA256			(3 << 3)
#define SFAX8_SHAM_HMAC_MD5			(4 << 3)
#define SFAX8_SHAM_HMAC_SHA1		(5 << 3)
#define SFAX8_SHAM_HMAC_SHA224		(6 << 3)
#define SFAX8_SHAM_HMAC_SHA256		(7 << 3)

#endif
