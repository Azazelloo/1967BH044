#include "H.h"
#include "DDUC.h"

/*Режим 22/10 - увелечение DXM до 22 бит, и  уменьшение DXC до 10 бит.
Увеличивает смещение адреса >32768 */

#define DDUC_BASE0_STEP_OFFSET (*(uint32_t*)(DDUC_BASE0+DDUC_STEP_OFFSET))
#define DDUC_BASE0_CR_OFFSET (*(uint32_t*)(DDUC_BASE0+DDUC_CR_OFFSET))

#define N 32768//КОЛИЧЕСТВО ЭЛЕМЕНТОВ В МАССИВЕ ПРИЁМНИКА
#define DMA_CH_RX 8//НОМЕР DMA ПРИЁМНИКА ( УСТРОЙСТВО -> ПАМЯТЬ )
#define DMA_CH_TX 7//НОМЕР DMA ПЕРЕДАТЧИКА ( ПАМЯТЬ -> УСТРОЙСТВО )

/*DMA_RX*/
uint32_t __attribute((aligned(4))) TCB_RX[4] = {0,};//DMA_RX
uint32_t __attribute((aligned(4))) TCB_TX[4] = {0,};//DMA_TX

static  uint16_t __attribute__((aligned(4))) BUF_TX_RX[N];

/*Определение типа памяти для контроллера DMA*/
// Сделать макросом!!!
int32_t HAL_DMA_INIT_MEM_TYPE(void* addr )
{
   if ((addr >= (0x00000000) && addr <= (0x0000FFFF)) || (addr >= 0x00040000 && addr <= (0x04FFFF)) ||	(addr >= 0x080000 && addr <= (0x08FFFF)) || (addr >= 0x0C0000 && addr <= (0x0CFFFF)) ||	(addr >= 0x100000 && addr <= (0x10FFFF)) || (addr >= 0x140000 && addr <= (0x14FFFF)))
	{ puts("INTMEM"); return TCB_INTMEM16_16; }
   else if((addr >= 0x30000000 && addr <= (0x44000000) ) || (addr >= 0x50000000 && addr <= (0x54000000)) || (addr >= 0x60000000 && addr <= (0x64000000)) || (addr >= 0x70000000 && addr <= (0x74000000)) || (addr >= 0x80000000 && addr <= (0xFFFFFFFF)))
	{ puts("EXTMEM"); return TCB_EXTMEM16_16; }
   else return -1;
}

int main(int argc , char* argv[])
{
  size_t DMA_CNT = 1;
  size_t IQ_CNT  = 0;
  size_t ErrFlag = 0;
  static FILE *FP_IN;
  static FILE *FP_OUT;
  uint32_t TCB_TYPE_MEMORY = 0;
  uint32_t dducregs[8];
  
  /*OPEN_READ_FILE_DATA*/
  //FP_IN  = fopen("W:/ML/MSO/DMA_UP_DOWN_CONVERTER/itest1.bin","r+b");
  FP_IN  = fopen("W:/ML/MSO/DMA_UP_DOWN_CONVERTER/IDDUC.bin","r+b");
  if(FP_IN == 0){ puts("NOT OPEN FILE itest1.bin") ; return -1; }

  /*WRITE_FILE_DATA*/
  FP_OUT = fopen("W:/ML/MSO/DMA_UP_DOWN_CONVERTER/ODDUC.bin","w+b");
  if(FP_OUT == 0){ puts("NOT OPEN FILE ODDUC.bin") ; fclose(FP_IN); return -1;}
  
  /*SETTINGS UP/DOWN*/
  DDUC_BASE0_STEP_OFFSET = 23593;//REG_STEP
  DDUC_BASE0_CR_OFFSET = SR_DDUC_ENABLE    |
                         SR_FIR_TAP*15     |
                         SR_SRC_DR         |
                         SR_ROUND_OFF      |
                         SR_SATURATION_OFF |
                         SR_ROUNDM_OFF     |
                         SR_FIR_ORDER3     |
                         SR_INPUT_BE       |
                         SR_OUTPUT_LE      |
                         SR_MODE_DDC       |
                         SR_SHIFT*15;// default - 0x101
  
/*
  // data and regs DDUC incapsulation
  fread(dducregs,sizeof(dducregs),1,FP_IN) ;
  DDUC_BASE0_STEP_OFFSET = dducregs[0] ;
  DDUC_BASE0_CR_OFFSET   = dducregs[1] ;
*/
  
  /*SETTING DMA*/
  //ПРИЁМНИК
  TCB_TYPE_MEMORY = HAL_DMA_INIT_MEM_TYPE(&BUF_TX_RX);//ОПРЕДЕЛЕНИЕ ТИПА ИСПОЛЬЗУЕМОЙ ПАМЯТИ
  if(TCB_TYPE_MEMORY == -1){ puts("ERROR TYPE MEMORY - RX"); }
  HAL_DMA_RqstClr(DMA_CH_RX);//ОЧИСТКА ИСТОЧНИКА ЗАПРОСА DMA
  HAL_DMA_RqstSet(DMA_CH_RX, dmaUPDOWN0);//УСТАНОВКА НОВОГО ИСТОЧНИКА ОПРОСА ДЛЯ КАНАЛА DMA_CH_RX
  TCB_RX[0] = ((void *)BUF_TX_RX);            //DI
  TCB_RX[1] = ((1020) << 16) | 4;             //DX 0xFFC0-MAX <----для 22/10
  TCB_RX[2] = 0;                              //DY
  TCB_RX[3] |= ( TCB_TYPE_MEMORY | TCB_QUAD); //DP
  //ПЕРЕГРУЗКА TX
  //HAL_DMA_CreateChannelDest(DMA_CH_RX,&TCB_RX,&TCB_RX);

  //ПЕРЕДАТЧИК
  TCB_TYPE_MEMORY = HAL_DMA_INIT_MEM_TYPE(&BUF_TX_RX);//ОПРЕДЕЛЕНИЕ ТИПА ИСПОЛЬЗУЕМОЙ ПАМЯТИ
  if(TCB_TYPE_MEMORY == -1){ puts("ERROR TYPE MEMORY - TX") ; }
  HAL_DMA_RqstClr(DMA_CH_TX);//ОЧИСТКА ИСТОЧНИКА ЗАПРОСА DMA
  HAL_DMA_RqstSet(DMA_CH_TX, dmaUPDOWN0);//УСТАНОВКА НОВОГО ИСТОЧНИКА ОПРОСА ДЛЯ КАНАЛА DMA_CH_TX
  TCB_TX[0] = ((void *)BUF_TX_RX);            //DI
  TCB_TX[1] = ((N/2) << 16) | 4;              //DX
  TCB_TX[2] = 0;                              //DY
  TCB_TX[3] |= ( TCB_TYPE_MEMORY | TCB_QUAD) ;//DP

  //HAL_DMA_WriteDC(DMA_CH_RX, &TCB_RX);//ЗАПИСЬ СТРУКТУРЫ В РЕГИСТР TCB И ЗАПУСК РАБОТЫ КАНАЛА DMA_CH_RX
  
  while(fread((void *)BUF_TX_RX,sizeof(uint16_t),N,FP_IN))
  {
    /*WRITE_SETTING_IN_DMA*/
    HAL_DMA_Stop( DMA_CH_TX );//ВЫКЛЮЧЕНИЕ DMA ПЕРЕДАТЧИКА
    HAL_DMA_Stop( DMA_CH_RX );//ВЫКЛЮЧЕНИЕ DMA ПРИЁМНИКА-------------------------------------- НЕ КОРРЕКТНО РАБОТАЕТ ЕСЛИ НЕ ВЫКЛЮЧАТЬ!!!!!!
    
    HAL_DMA_GetChannelStatusClear( DMA_CH_TX );
    HAL_DMA_GetChannelStatusClear( DMA_CH_RX );
    
    HAL_DMA_WriteDC(DMA_CH_TX, &TCB_TX);//ЗАПИСЬ СТРУКТУРЫ В РЕГИСТР TCB И ЗАПУСК РАБОТЫ КАНАЛА DMA_CH_TX
    HAL_DMA_WriteDC(DMA_CH_RX, &TCB_RX);//ЗАПИСЬ СТРУКТУРЫ В РЕГИСТР TCB И ЗАПУСК РАБОТЫ КАНАЛА DMA_CH_TX
    
    /*MAIN_1*/
    ErrFlag = HAL_DMA_WaitForChannel(DMA_CH_RX);
    if(ErrFlag == 0)	 { printf("OK TX DMA - %i\n",DMA_CNT++); }
    else if(ErrFlag == 1){ puts("ERROR TX DMA") ; return -1;     }
    else if(ErrFlag == 2){ puts("TX DMA OFF")   ; return -1;     }

    /*GET COUNT DMA*/
    //IQ_CNT = HAL_DMA_GetDcCountX(DMA_CH_RX);//&0x7FFF<----из-за 22/10
    
    /*WRITE_FILE_DATA*/
    fwrite((void *)BUF_TX_RX,sizeof(uint32_t),1020,FP_OUT) ;
  }
  /*CLOSE_FILE*/
  fclose(FP_OUT);
  fclose(FP_IN);
  puts("END") ;
  return 0;
}
