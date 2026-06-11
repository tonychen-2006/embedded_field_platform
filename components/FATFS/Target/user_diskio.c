/* USER CODE BEGIN Header */
/**
 ******************************************************************************
  * @file    user_diskio.c
  * @brief   This file includes a diskio driver skeleton to be completed by the user.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
 /* USER CODE END Header */

#ifdef USE_OBSOLETE_USER_CODE_SECTION_0
/*
 * Warning: the user section 0 is no more in use (starting from CubeMx version 4.16.0)
 * To be suppressed in the future.
 * Kept to ensure backward compatibility with previous CubeMx versions when
 * migrating projects.
 * User code previously added there should be copied in the new user sections before
 * the section contents can be deleted.
 */
/* USER CODE BEGIN 0 */
/* USER CODE END 0 */
#endif

/* USER CODE BEGIN DECL */

/* Includes ------------------------------------------------------------------*/
#include <stdbool.h>
#include <string.h>
#include "ff_gen_drv.h"
#include "main.h"

/* Private define ------------------------------------------------------------*/
#define SD_BLOCK_SIZE              512U
#define SD_DUMMY_BYTE              0xFFU
#define SD_START_TOKEN             0xFEU
#define SD_DATA_ACCEPTED           0x05U

#define SD_CMD0_GO_IDLE            0U
#define SD_CMD8_IF_COND            8U
#define SD_CMD16_SET_BLOCKLEN      16U
#define SD_CMD17_READ_BLOCK        17U
#define SD_CMD24_WRITE_BLOCK       24U
#define SD_CMD55_APP_CMD           55U
#define SD_CMD58_READ_OCR          58U
#define SD_ACMD41_INIT             41U

#define SD_TIMEOUT_MS              500U
#define SD_INIT_TIMEOUT_MS         2000U
#define SD_SPI_TIMEOUT_MS          100U

/* Private variables ---------------------------------------------------------*/
static volatile DSTATUS Stat = STA_NOINIT;
static uint8_t sd_is_sdhc;

extern SPI_HandleTypeDef hspi1;

static uint8_t SD_SPI(uint8_t data);
static void SD_Select(void);
static void SD_Deselect(void);
static bool SD_WaitReady(uint32_t timeout_ms);
static uint8_t SD_Command(uint8_t cmd, uint32_t arg, uint8_t crc);
static uint8_t SD_AppCommand(uint8_t cmd, uint32_t arg);
static bool SD_InitCard(void);
static bool SD_ReadSingleBlock(DWORD sector, BYTE *buff);
static bool SD_WriteSingleBlock(DWORD sector, const BYTE *buff);

static uint8_t SD_SPI(uint8_t data)
{
  uint8_t received = SD_DUMMY_BYTE;

  (void)HAL_SPI_TransmitReceive(&hspi1, &data, &received, 1U, SD_SPI_TIMEOUT_MS);
  return received;
}

static void SD_Select(void)
{
  HAL_GPIO_WritePin(SD_CS_GPIO_Port, SD_CS_Pin, GPIO_PIN_RESET);
}

static void SD_Deselect(void)
{
  HAL_GPIO_WritePin(SD_CS_GPIO_Port, SD_CS_Pin, GPIO_PIN_SET);
  (void)SD_SPI(SD_DUMMY_BYTE);
}

static bool SD_WaitReady(uint32_t timeout_ms)
{
  uint32_t start_ms = HAL_GetTick();

  while ((HAL_GetTick() - start_ms) < timeout_ms)
  {
    if (SD_SPI(SD_DUMMY_BYTE) == SD_DUMMY_BYTE)
    {
      return true;
    }
  }

  return false;
}

static uint8_t SD_Command(uint8_t cmd, uint32_t arg, uint8_t crc)
{
  uint8_t response;

  SD_Deselect();
  SD_Select();

  if (!SD_WaitReady(SD_TIMEOUT_MS))
  {
    return SD_DUMMY_BYTE;
  }

  (void)SD_SPI(0x40U | cmd);
  (void)SD_SPI((uint8_t)(arg >> 24));
  (void)SD_SPI((uint8_t)(arg >> 16));
  (void)SD_SPI((uint8_t)(arg >> 8));
  (void)SD_SPI((uint8_t)arg);
  (void)SD_SPI(crc);

  for (uint8_t i = 0U; i < 10U; i++)
  {
    response = SD_SPI(SD_DUMMY_BYTE);
    if ((response & 0x80U) == 0U)
    {
      return response;
    }
  }

  return SD_DUMMY_BYTE;
}

static uint8_t SD_AppCommand(uint8_t cmd, uint32_t arg)
{
  uint8_t response;

  response = SD_Command(SD_CMD55_APP_CMD, 0U, 0x01U);
  SD_Deselect();
  if (response > 1U)
  {
    return response;
  }

  return SD_Command(cmd, arg, 0x01U);
}

static bool SD_InitCard(void)
{
  uint32_t start_ms;
  uint8_t response;
  uint8_t ocr[4];

  sd_is_sdhc = 0U;

  SD_Deselect();
  for (uint8_t i = 0U; i < 10U; i++)
  {
    (void)SD_SPI(SD_DUMMY_BYTE);
  }

  start_ms = HAL_GetTick();
  do
  {
    response = SD_Command(SD_CMD0_GO_IDLE, 0U, 0x95U);
    SD_Deselect();
  } while (response != 0x01U &&
           (HAL_GetTick() - start_ms) < SD_INIT_TIMEOUT_MS);

  if (response != 0x01U)
  {
    return false;
  }

  response = SD_Command(SD_CMD8_IF_COND, 0x000001AAU, 0x87U);
  if (response != 0x01U)
  {
    SD_Deselect();
    return false;
  }

  for (uint8_t i = 0U; i < sizeof(ocr); i++)
  {
    ocr[i] = SD_SPI(SD_DUMMY_BYTE);
  }
  SD_Deselect();

  if (ocr[2] != 0x01U || ocr[3] != 0xAAU)
  {
    return false;
  }

  start_ms = HAL_GetTick();
  do
  {
    response = SD_AppCommand(SD_ACMD41_INIT, 0x40000000U);
    SD_Deselect();
  } while (response != 0x00U &&
           (HAL_GetTick() - start_ms) < SD_INIT_TIMEOUT_MS);

  if (response != 0x00U)
  {
    return false;
  }

  response = SD_Command(SD_CMD58_READ_OCR, 0U, 0x01U);
  if (response != 0x00U)
  {
    SD_Deselect();
    return false;
  }

  for (uint8_t i = 0U; i < sizeof(ocr); i++)
  {
    ocr[i] = SD_SPI(SD_DUMMY_BYTE);
  }
  SD_Deselect();

  sd_is_sdhc = (ocr[0] & 0x40U) != 0U ? 1U : 0U;
  if (sd_is_sdhc == 0U)
  {
    response = SD_Command(SD_CMD16_SET_BLOCKLEN, SD_BLOCK_SIZE, 0x01U);
    SD_Deselect();
    if (response != 0x00U)
    {
      return false;
    }
  }

  return true;
}

static bool SD_ReadSingleBlock(DWORD sector, BYTE *buff)
{
  uint32_t address = sd_is_sdhc != 0U ? sector : sector * SD_BLOCK_SIZE;
  uint32_t start_ms;

  if (SD_Command(SD_CMD17_READ_BLOCK, address, 0x01U) != 0x00U)
  {
    SD_Deselect();
    return false;
  }

  start_ms = HAL_GetTick();
  while (SD_SPI(SD_DUMMY_BYTE) != SD_START_TOKEN)
  {
    if ((HAL_GetTick() - start_ms) >= SD_TIMEOUT_MS)
    {
      SD_Deselect();
      return false;
    }
  }

  for (uint16_t i = 0U; i < SD_BLOCK_SIZE; i++)
  {
    buff[i] = SD_SPI(SD_DUMMY_BYTE);
  }

  (void)SD_SPI(SD_DUMMY_BYTE);
  (void)SD_SPI(SD_DUMMY_BYTE);
  SD_Deselect();

  return true;
}

static bool SD_WriteSingleBlock(DWORD sector, const BYTE *buff)
{
  uint32_t address = sd_is_sdhc != 0U ? sector : sector * SD_BLOCK_SIZE;
  uint8_t response;

  if (SD_Command(SD_CMD24_WRITE_BLOCK, address, 0x01U) != 0x00U)
  {
    SD_Deselect();
    return false;
  }

  (void)SD_SPI(SD_START_TOKEN);
  for (uint16_t i = 0U; i < SD_BLOCK_SIZE; i++)
  {
    (void)SD_SPI(buff[i]);
  }

  (void)SD_SPI(SD_DUMMY_BYTE);
  (void)SD_SPI(SD_DUMMY_BYTE);

  response = SD_SPI(SD_DUMMY_BYTE);
  if ((response & 0x1FU) != SD_DATA_ACCEPTED)
  {
    SD_Deselect();
    return false;
  }

  if (!SD_WaitReady(SD_TIMEOUT_MS))
  {
    SD_Deselect();
    return false;
  }

  SD_Deselect();
  return true;
}

/* USER CODE END DECL */

/* Private function prototypes -----------------------------------------------*/
DSTATUS USER_initialize (BYTE pdrv);
DSTATUS USER_status (BYTE pdrv);
DRESULT USER_read (BYTE pdrv, BYTE *buff, DWORD sector, UINT count);
#if _USE_WRITE == 1
  DRESULT USER_write (BYTE pdrv, const BYTE *buff, DWORD sector, UINT count);
#endif /* _USE_WRITE == 1 */
#if _USE_IOCTL == 1
  DRESULT USER_ioctl (BYTE pdrv, BYTE cmd, void *buff);
#endif /* _USE_IOCTL == 1 */

Diskio_drvTypeDef  USER_Driver =
{
  USER_initialize,
  USER_status,
  USER_read,
#if  _USE_WRITE
  USER_write,
#endif  /* _USE_WRITE == 1 */
#if  _USE_IOCTL == 1
  USER_ioctl,
#endif /* _USE_IOCTL == 1 */
};

/* Private functions ---------------------------------------------------------*/

/**
  * @brief  Initializes a Drive
  * @param  pdrv: Physical drive number (0..)
  * @retval DSTATUS: Operation status
  */
DSTATUS USER_initialize (
	BYTE pdrv           /* Physical drive nmuber to identify the drive */
)
{
  /* USER CODE BEGIN INIT */
  if (pdrv != 0U)
  {
    return STA_NOINIT;
  }

  Stat = SD_InitCard() ? 0U : STA_NOINIT;
  return Stat;
  /* USER CODE END INIT */
}

/**
  * @brief  Gets Disk Status
  * @param  pdrv: Physical drive number (0..)
  * @retval DSTATUS: Operation status
  */
DSTATUS USER_status (
	BYTE pdrv       /* Physical drive number to identify the drive */
)
{
  /* USER CODE BEGIN STATUS */
  (void)pdrv;
  return Stat;
  /* USER CODE END STATUS */
}

/**
  * @brief  Reads Sector(s)
  * @param  pdrv: Physical drive number (0..)
  * @param  *buff: Data buffer to store read data
  * @param  sector: Sector address (LBA)
  * @param  count: Number of sectors to read (1..128)
  * @retval DRESULT: Operation result
  */
DRESULT USER_read (
	BYTE pdrv,      /* Physical drive nmuber to identify the drive */
	BYTE *buff,     /* Data buffer to store read data */
	DWORD sector,   /* Sector address in LBA */
	UINT count      /* Number of sectors to read */
)
{
  /* USER CODE BEGIN READ */
  if (pdrv != 0U || buff == NULL || count == 0U)
  {
    return RES_PARERR;
  }

  if ((Stat & STA_NOINIT) != 0U)
  {
    return RES_NOTRDY;
  }

  for (UINT i = 0U; i < count; i++)
  {
    if (!SD_ReadSingleBlock(sector + i, &buff[i * SD_BLOCK_SIZE]))
    {
      return RES_ERROR;
    }
  }

  return RES_OK;
  /* USER CODE END READ */
}

/**
  * @brief  Writes Sector(s)
  * @param  pdrv: Physical drive number (0..)
  * @param  *buff: Data to be written
  * @param  sector: Sector address (LBA)
  * @param  count: Number of sectors to write (1..128)
  * @retval DRESULT: Operation result
  */
#if _USE_WRITE == 1
DRESULT USER_write (
	BYTE pdrv,          /* Physical drive nmuber to identify the drive */
	const BYTE *buff,   /* Data to be written */
	DWORD sector,       /* Sector address in LBA */
	UINT count          /* Number of sectors to write */
)
{
  /* USER CODE BEGIN WRITE */
  if (pdrv != 0U || buff == NULL || count == 0U)
  {
    return RES_PARERR;
  }

  if ((Stat & STA_NOINIT) != 0U)
  {
    return RES_NOTRDY;
  }

  for (UINT i = 0U; i < count; i++)
  {
    if (!SD_WriteSingleBlock(sector + i, &buff[i * SD_BLOCK_SIZE]))
    {
      return RES_ERROR;
    }
  }

  return RES_OK;
  /* USER CODE END WRITE */
}
#endif /* _USE_WRITE == 1 */

/**
  * @brief  I/O control operation
  * @param  pdrv: Physical drive number (0..)
  * @param  cmd: Control code
  * @param  *buff: Buffer to send/receive control data
  * @retval DRESULT: Operation result
  */
#if _USE_IOCTL == 1
DRESULT USER_ioctl (
	BYTE pdrv,      /* Physical drive nmuber (0..) */
	BYTE cmd,       /* Control code */
	void *buff      /* Buffer to send/receive control data */
)
{
  /* USER CODE BEGIN IOCTL */
  if (pdrv != 0U)
  {
    return RES_PARERR;
  }

  if ((Stat & STA_NOINIT) != 0U)
  {
    return RES_NOTRDY;
  }

  switch (cmd)
  {
  case CTRL_SYNC:
    SD_Select();
    if (SD_WaitReady(SD_TIMEOUT_MS))
    {
      SD_Deselect();
      return RES_OK;
    }
    SD_Deselect();
    return RES_ERROR;

  case GET_SECTOR_COUNT:
    if (buff == NULL)
    {
      return RES_PARERR;
    }
    *(DWORD *)buff = 0U;
    return RES_OK;

  case GET_SECTOR_SIZE:
    if (buff == NULL)
    {
      return RES_PARERR;
    }
    *(WORD *)buff = SD_BLOCK_SIZE;
    return RES_OK;

  case GET_BLOCK_SIZE:
    if (buff == NULL)
    {
      return RES_PARERR;
    }
    *(DWORD *)buff = 1U;
    return RES_OK;

  default:
    return RES_PARERR;
  }
  /* USER CODE END IOCTL */
}
#endif /* _USE_IOCTL == 1 */

