#include "sys.h"
#include "usart.h"
//////////////////////////////////////////////////////////////////////////////////
//如果使用ucos,则包括下面的头文件即可.
#if SYSTEM_SUPPORT_UCOS
#include "includes.h"					//ucos 使用	  
#endif

//////////////////////////////////////////////////////////////////
//加入以下代码,支持printf函数,而不需要选择use MicroLIB
void usart_ctrl_music(void);
#if 1
#pragma import(__use_no_semihosting)
//标准库需要的支持函数
struct __FILE
{
	int handle;
};

FILE __stdout;
//定义_sys_exit()以避免使用半主机模式
_sys_exit(int x)
{
	x = x;
}
//重定义fputc函数
int fputc(int ch, FILE *f)
{
	while((USART3->SR&0X40)==0);//循环发送,直到发送完毕
	USART3->DR = (u8) ch;
	return ch;
}
#endif

#if EN_USART3_RX   //如果使能了接收
//串口3中断服务程序
//注意,读取USARTx->SR能避免莫名其妙的错误
u8 USART_RX_BUF[USART_REC_LEN];     //接收缓冲,最大USART_REC_LEN个字节.
u8  USART_RX_TEMP[USART_REC_LEN];	//临时接收数据，供外部调用
//接收状态
//bit15，	接收完成标志
//bit14，	接收到0x0d
//bit13~0，	接收到的有效字节数目
u16 USART_RX_STA=0;       //接收状态标记

//初始化IO 串口3
//bound:波特率
void uart_init(u32 bound)
{
	//GPIO端口设置
	GPIO_InitTypeDef GPIO_InitStructure;
	USART_InitTypeDef USART_InitStructure;
	NVIC_InitTypeDef NVIC_InitStructure;

	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB,ENABLE); //使能GPIOB时钟
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART3,ENABLE);//使能USART3时钟

	//串口3对应引脚复用映射
	GPIO_PinAFConfig(GPIOB,GPIO_PinSource10,GPIO_AF_USART3); //GPIOB10复用为USART3
	GPIO_PinAFConfig(GPIOB,GPIO_PinSource11,GPIO_AF_USART3); //GPIOB11复用为USART3

	//USART3端口配置
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10 | GPIO_Pin_11; //GPIOB10与GPIOB11
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;			//复用功能
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;		//速度50MHz
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP; 			//推挽复用输出
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP; 			//上拉
	GPIO_Init(GPIOB,&GPIO_InitStructure); 					//初始化PB10，PB11

	//USART3 初始化设置
	USART_InitStructure.USART_BaudRate = bound;					//波特率设置
	USART_InitStructure.USART_WordLength = USART_WordLength_8b;//字长为8位数据格式
	USART_InitStructure.USART_StopBits = USART_StopBits_1;		//一个停止位
	USART_InitStructure.USART_Parity = USART_Parity_No;			//无奇偶校验位
	USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;//无硬件数据流控制
	USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;	//收发模式
	USART_Init(USART3, &USART_InitStructure); 					//初始化串口3

	USART_Cmd(USART3, ENABLE);  							//使能串口3

	USART_ClearFlag(USART3, USART_FLAG_TC);

#if EN_USART3_RX
	USART_ITConfig(USART3, USART_IT_RXNE, ENABLE);			//开启相关中断

	//Usart1 NVIC 配置
	NVIC_InitStructure.NVIC_IRQChannel = USART3_IRQn;		//串口3中断通道
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority=3;//抢占优先级3
	NVIC_InitStructure.NVIC_IRQChannelSubPriority =3;		//子优先级3
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;			//IRQ通道使能
	NVIC_Init(&NVIC_InitStructure);							//根据指定的参数初始化VIC寄存器、

#endif

}

//串口3中断服务程序
void USART3_IRQHandler(void)                	
{
	u8 Res;
#ifdef OS_TICKS_PER_SEC	 	//如果时钟节拍数定义了,说明要使用ucosII了.
	OSIntEnter();
#endif
	if(USART_GetITStatus(USART3, USART_IT_RXNE) != RESET)  //接收中断(接收到的数据必须是0x0d 0x0a结尾)
	{
		Res =USART_ReceiveData(USART3);//(USART3->DR);	//读取接收到的数据

		if((USART_RX_STA&0x8000)==0)//接收未完成
		{
			if(USART_RX_STA&0x4000)//接收到了0x0d
			{
				if(Res!=0x0a)
					USART_RX_STA=0;//接收错误,重新开始
				else
				{
					USART_RX_STA|=0x8000;	//接收完成了
					strcpy((char*)USART_RX_TEMP,(char*)USART_RX_BUF);//copy接收到的数据，供外部调用
					strncpy((char*)USART_RX_BUF," ",30);//清空USART_RX_BUF
					usart_ctrl_music();					//串口输入命令控制音乐播放
					USART_RX_STA=0;
				}
			}
			else //还没收到0X0D
			{
				if(Res==0x0d)USART_RX_STA|=0x4000;
				else
				{
					USART_RX_BUF[USART_RX_STA&0X3FFF]=Res ;
					USART_RX_STA++;
					if(USART_RX_STA>(USART_REC_LEN-1))
						USART_RX_STA=0;//接收数据错误,重新开始接收
				}
			}
		}
	}
#ifdef OS_TICKS_PER_SEC	 	//如果时钟节拍数定义了,说明要使用ucosII了.
	OSIntExit();
#endif
}

void usart_ctrl_music(void)
{
	if(!strncmp((char*)USART_RX_TEMP,"MUSIC_MODE",11))
	{
		MUSIC_MODE_KEY = 1;
		delay_ms(100);
		MUSIC_MODE_KEY = 0;
	}
	else if(!strncmp((char*)USART_RX_TEMP,"PAGE_DOWN",9))
	{
		PAGE_DOWN_KEY = 0;
		delay_ms(100);
		PAGE_DOWN_KEY = 1;
	}
	else if(!strncmp((char*)USART_RX_TEMP,"NEXT_SONG",9))
	{
		NEXT_SONG_KEY = 0;
		delay_ms(100);
		NEXT_SONG_KEY = 1;
	}
	else if(!strncmp((char*)USART_RX_TEMP,"VOL_UP",6))
	{
		VOL_UP_KEY = 0;
		delay_ms(100);
		VOL_UP_KEY = 1;
	}
	else if(!strncmp((char*)USART_RX_TEMP,"PAUSE_SONG",11))
	{
		PAUSE_SONG_KEY = 0;
		delay_ms(100);
		PAUSE_SONG_KEY = 1;
	}
	else if(!strncmp((char*)USART_RX_TEMP,"VOL_DOWN",8))
	{
		VOL_DOWN_KEY = 0;
		delay_ms(100);
		VOL_DOWN_KEY = 1;
	}
	else if(!strncmp((char*)USART_RX_TEMP,"STOP_SONG",9))
	{
		STOP_SONG_KEY = 0;
		delay_ms(100);
		STOP_SONG_KEY = 1;
	}
	else if(!strncmp((char*)USART_RX_TEMP,"PAGE_UP",7))
	{
		PAGE_UP_KEY = 0;
		delay_ms(100);
		PAGE_UP_KEY = 1;
	}
	else if(!strncmp((char*)USART_RX_TEMP,"LAST_SONG",9))
	{
		LAST_SONG_KEY = 0;
		delay_ms(100);
		LAST_SONG_KEY = 1;
	}
	else
		return;
}
#endif





