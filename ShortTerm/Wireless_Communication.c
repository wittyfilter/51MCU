#include <reg52.h>
#include <intrins.h>
#include "api.h"

#define uchar unsigned char

/***************************************************/
#define TX_ADR_WIDTH   5  // 5字节宽度的发送/接收地址
#define TX_PLOAD_WIDTH 10  // 数据通道有效数据宽度

//定义一个静态发送地址
uchar code TX_ADDRESS[TX_ADR_WIDTH] = {0x34,0x43,0x10,0x10,0x01}; 
uchar RX_BUF[TX_PLOAD_WIDTH];			 //4 byte宽度的接收缓存
uchar TX_BUF[TX_PLOAD_WIDTH];			 //4 byte宽度的发射缓存
uchar flag;								 //标志位
uchar DATA = 0x01;
uchar bdata sta;
uchar i, n = 0;					 	 //定义可位寻址的变量sta
sbit RX_DR = sta^6;
sbit TX_DS = sta^5;
sbit MAX_RT = sta^4;
sbit CE = P1^0;
sbit CSN = P1^1;
sbit SCK = P1^2;
sbit MOSI = P1^3;
sbit MISO = P1^4;
sbit IRQ = P1^6;
sbit IRIN = P3^2;	//红外接收 
sbit BEEP = P0^4;   //蜂鸣器

uchar set1, set2, number;
uchar IRCOM[7];
bit flag_keybeep, senddata, flag_number;

/******************************************/
/*				延时函数
/******************************************/
void delay(uchar x)    //x*0.14MS
{
	uchar i;
	while(x--){
		for(i=0;i<13;i++) {}
	}
}

void delayms(uchar count)//参数为毫秒数
{
	uchar i,j;
	for(i=0;i<count;i++)
		for(j=0;j<240;j++); 
} 

/***********************************/
/*			按键音函数
/***********************************/
void beep()
{
	uchar i;
	for(i=0;i<100;i++){
		delay(4);
		BEEP = ~BEEP;             //BEEP取反
	} 
	BEEP = 1;                     //关闭蜂鸣器
}


/****************************/
/*			LCD 1602
/****************************/					
sfr		 io	= 0xA0;				//P0-0x80,P1-0x90,P2-0xA0,P3-0xB0;
sbit	 rs = P0^7;				//LCD数据/命令选择端(H/L)
sbit	 rw = P0^6;				//LCD读/写选择端(H/L)
sbit	 ep = P0^5;				//LCD使能控制
sbit     bz = io^7;				//LCD忙标志位

//------------------------测试LCD忙碌状态-----------------------//
void lcd_busy(void)
{	
	do{
		ep = 0;
		rs = 0;		//指令
		rw = 1;		//读出
		io = 0xff;
		ep = 1;
		_nop_();	//高电平读出	1us	
	}while(bz);		//bz=1表示忙,bz=0表示空闲
	ep = 0;		
}

//--------------------------写入指令到LCD----------------------------//
void lcd_wcmd(uchar cmd)
{							
	lcd_busy();	//检测忙
	rs = 0;		//指令
	rw = 0;		//写入
	ep = 1;	
	io = cmd;	//指令
	ep = 0;		//下降沿有效	
}

//-------------------------LCD数据指针位置程序-----------------------//
void lcd_pos(uchar x, bit y)
{						
	if(y)lcd_wcmd(x|0xc0);	//y=1,第二行显示;y=0,第一行显示		0<=x<16
	else lcd_wcmd(x|0x80);	//数据指针=80+地址码(00H~27H,40H~67H)
}

//----------------------------写入数据函数------------------------//
void lcd_wdat(unsigned char Data)
{
	lcd_busy();  //检测忙
	rs = 1;		 //数据
	rw = 0;		 //写入
	ep = 1;
	io = Data;	 //数据
	ep = 0;		 //下降沿有效
}

//---------------------------显示字符串---------------------------//
void prints(uchar *string)
{
	while(*string) {lcd_wdat(*string);string++;}
}

//--------------------------LCD初始化设定-------------------------//
void lcd_init()
{						
	lcd_wcmd(0x38);		//设置LCD为16X2显示,5X7点阵,八位数据接口
	lcd_wcmd(0x06);		//LCD显示光标移动设置(光标地址指针加1,整屏显示不移动)
	lcd_wcmd(0x0c);		//LCD开显示及光标设置(光标不闪烁,不显示"_")
	lcd_wcmd(0x01);		//清除LCD的显示内容
}

//--------------------------数字转字符函数-----------------------//
void display(uchar x,uchar i,uchar j)
{
	uchar datachar[2];
	datachar[0] = x%10 + '0';
	datachar[1] = '\0';
   	lcd_pos(6+i,j);
	prints(datachar);	
}

/**************************************************
函数: init_io()

描述: 初始化IO
/**************************************************/
void init_io(void)
{
	CE  = 0;        // 待机
	CSN = 1;        // SPI禁止
	SCK = 0;        // SPI时钟置低
	IRQ = 1;        // 中断复位
}

/**************************************************
函数：delay_ms()

描述：延迟x毫秒
/**************************************************/
void delay_ms(uchar x)
{
    uchar i, j;
    i = 0;
    for(i=0; i<x; i++){
       j = 250;
       while(--j);
	   j = 250;
       while(--j);
    }
}

/**************************************************
函数：SPI_RW()

描述：根据SPI协议，写一字节数据到nRF24L01，同时从nRF24L01
读出一字节
/**************************************************/
uchar SPI_RW(uchar byte)
{
	uchar i;
   	for(i=0; i<8; i++){         // 循环8次
   		MOSI = (byte & 0x80);   // byte最高位输出到MOSI
   		byte <<= 1;             // 低一位移位到最高位
   		SCK = 1;                // 拉高SCK，nRF24L01从MOSI读入1位数据，同时从MISO输出1位数据
   		byte |= MISO;       	// 读MISO到byte最低位
   		SCK = 0;            	// SCK置低
   	}
    return(byte);           	// 返回读出的一字节
}

/**************************************************
函数：SPI_RW_Reg()

描述：写数据value到reg寄存器
/**************************************************/
uchar SPI_RW_Reg(uchar reg, uchar value)
{
	uchar status;
  	CSN = 0;                   // CSN置低，开始传输数据
  	status = SPI_RW(reg);      // 选择寄存器，同时返回状态字
  	SPI_RW(value);             // 然后写数据到该寄存器
  	CSN = 1;                   // CSN拉高，结束数据传输
  	return(status);            // 返回状态寄存器
}

/**************************************************
函数：SPI_Read()

描述：从reg寄存器读一字节
/**************************************************/
uchar SPI_Read(uchar reg)
{
	uchar reg_val;
  	CSN = 0;                    // CSN置低，开始传输数据
  	SPI_RW(reg);                // 选择寄存器
  	reg_val = SPI_RW(0);        // 然后从该寄存器读数据
  	CSN = 1;                    // CSN拉高，结束数据传输
  	return(reg_val);            // 返回寄存器数据
}

/**************************************************
函数：SPI_Read_Buf()

描述：从reg寄存器读出bytes个字节，通常用来读取接收通道
数据或接收/发送地址
/**************************************************/
uchar SPI_Read_Buf(uchar reg, uchar * pBuf, uchar bytes)
{
	uchar status, i;
  	CSN = 0;                    // CSN置低，开始传输数据
  	status = SPI_RW(reg);       // 选择寄存器，同时返回状态字
  	for(i=0; i<bytes; i++)
    	pBuf[i] = SPI_RW(0);    // 逐个字节从nRF24L01读出
  	CSN = 1;                    // CSN拉高，结束数据传输
  	return(status);             // 返回状态寄存器
}

/**************************************************
函数：SPI_Write_Buf()

描述：把pBuf缓存中的数据写入到nRF24L01，通常用来写入发
射通道数据或接收/发送地址
/**************************************************/
uchar SPI_Write_Buf(uchar reg, uchar * pBuf, uchar bytes)
{
	uchar status, i;
  	CSN = 0;                    // CSN置低，开始传输数据
  	status = SPI_RW(reg);       // 选择寄存器，同时返回状态字
  	for(i=0; i<bytes; i++)
    	SPI_RW(pBuf[i]);        // 逐个字节写入nRF24L01
  	CSN = 1;                    // CSN拉高，结束数据传输
  	return(status);             // 返回状态寄存器
}

/**************************************************
函数：RX_Mode()

描述：这个函数设置nRF24L01为接收模式，等待接收发送设备的数据包
/**************************************************/
void RX_Mode(void)
{
	CE = 0;
  	SPI_Write_Buf(WRITE_REG + RX_ADDR_P0, TX_ADDRESS, TX_ADR_WIDTH);  // 接收设备接收通道0使用和发送设备相同的发送地址
  	SPI_RW_Reg(WRITE_REG + EN_AA, 0x01);               // 使能接收通道0自动应答
  	SPI_RW_Reg(WRITE_REG + EN_RXADDR, 0x01);           // 使能接收通道0
  	SPI_RW_Reg(WRITE_REG + RF_CH, 40);                 // 选择射频通道0x40
  	SPI_RW_Reg(WRITE_REG + RX_PW_P0, TX_PLOAD_WIDTH);  // 接收通道0选择和发送通道相同有效数据宽度
  	SPI_RW_Reg(WRITE_REG + RF_SETUP, 0x07);            // 数据传输率1Mbps，发射功率0dBm，低噪声放大器增益
  	SPI_RW_Reg(WRITE_REG + CONFIG, 0x0f);              // CRC使能，16位CRC校验，上电，接收模式
  	CE = 1;                                            // 拉高CE启动接收设备
}

/**************************************************
函数：TX_Mode()

描述：这个函数设置nRF24L01为发送模式，（CE=1持续至少10us），
130us后启动发射，数据发送结束后，发送模块自动转入接收模式等待应答信号。
/**************************************************/
void TX_Mode(uchar * BUF)
{
	CE = 0;
  	SPI_Write_Buf(WRITE_REG + TX_ADDR, TX_ADDRESS, TX_ADR_WIDTH);     // 写入发送地址
  	SPI_Write_Buf(WRITE_REG + RX_ADDR_P0, TX_ADDRESS, TX_ADR_WIDTH);  // 为了应答接收设备，接收通道0地址和发送地址相同
  	SPI_Write_Buf(WR_TX_PLOAD, BUF, TX_PLOAD_WIDTH);                  // 写数据包到TX FIFO
  	SPI_RW_Reg(WRITE_REG + EN_AA, 0x01);       // 使能接收通道0自动应答
  	SPI_RW_Reg(WRITE_REG + EN_RXADDR, 0x01);   // 使能接收通道0
  	SPI_RW_Reg(WRITE_REG + SETUP_RETR, 0x0a);  // 自动重发延时等待250us+86us，自动重发10次
  	SPI_RW_Reg(WRITE_REG + RF_CH, 40);         // 选择射频通道0x40
  	SPI_RW_Reg(WRITE_REG + RF_SETUP, 0x07);    // 数据传输率1Mbps，发射功率0dBm，低噪声放大器增益
  	SPI_RW_Reg(WRITE_REG + CONFIG, 0x0e);      // CRC使能，16位CRC校验，上电
	CE = 1;
}

/**************************************************
函数：Check_ACK()

描述：检查接收设备有无接收到数据包，设定没有收到应答信
号是否重发
/**************************************************/
uchar Check_ACK(bit clear)
{
	while(IRQ);
	sta = SPI_RW(NOP);                    // 返回状态寄存器
	if(MAX_RT)
		if(clear)                         // 是否清除TX FIFO，没有清除在复位MAX_RT中断标志后重发
			SPI_RW(FLUSH_TX);
	SPI_RW_Reg(WRITE_REG + STATUS, sta);  // 清除TX_DS或MAX_RT中断标志
	IRQ = 1;
	if(TX_DS)
		return(0x00);
	else
		return(0xff);
}

/**************************************************
函数：CheckInput()

描述：检查输入，作出响应
/**************************************************/
void CheckInput()
{		
	if(flag_number){				//若输入数据    
		TX_BUF[i] = number;         // 数据送到缓存
		display(TX_BUF[i],i,0);		//显示
		flag_number = 0;
		i++;
	}
	if(senddata){					//若确认发送
		lcd_pos(6,0);
		prints("          ");	    //清除显示
		TX_BUF[i] = 0xff;           // 数据送到缓存
		TX_Mode(TX_BUF);			// 把nRF24L01设置为发送模式并发送数据
		Check_ACK(1);               // 等待发送完毕，清除TX FIFO
		delay_ms(250);
		delay_ms(250);
		RX_Mode();	        		// 设置为接收模式
		i = 0;
		senddata = 0;
	}
}

/*************************************************/
/*                   红外初始化                               
/*************************************************/
void IR_init(){
	IRIN = 1;
	BEEP = 1;
                 
	delayms(10);                 //延时
	IE = 0x81;                 //允许总中断中断,使能 INT0 外部中断
	TCON = 0x01;               //触发方式为脉冲负边沿触发
}

/**************************************************/
/*					主函数
/**************************************************/
void main(void)
{
	uchar j;
	lcd_init();
	IR_init();
	init_io();		              // 初始化IO
	RX_Mode();		              // 设置为接收模式
	lcd_pos(0,0);
	prints("Send:");
	lcd_pos(0,1);
	prints("Recv:");
	while(1){		
		CheckInput();             // 输入扫描
		sta = SPI_Read(STATUS);	  // 读状态寄存器
	    if(RX_DR){				  // 判断是否接受到数据
			SPI_Read_Buf(RD_RX_PLOAD, RX_BUF, TX_PLOAD_WIDTH);  // 从RX FIFO读出数据
			flag = 1;
		}
		SPI_RW_Reg(WRITE_REG + STATUS, sta);  // 清除RX_DS中断标志
		if(flag){		           // 接受完成
			j = 0;
			while(RX_BUF[j]!=0xff){
				display(RX_BUF[j],j,1);
				j++;
			}
			lcd_pos(6+j,1);
			prints("      ");
			flag = 0;		       // 清标志
			delay_ms(250);
			delay_ms(250);
		}
	}
}

/****************************************/
/*			外部中断0——红外
/****************************************/
void IR_IN() interrupt 0
{
	uchar j, k, N = 0;
	EX0 = 0;   
	delay(15);
	if(IRIN==1){
		EX0 = 1;
		return;
	} 
                           //确认IR信号出现
    while(!IRIN)
		delay(1);            //等IR变为高电平，跳过9ms的前导低电平信号。

	for(j=0;j<4;j++){         //收集四组数据
		for (k=0;k<8;k++){        //每组数据有8位
			while(IRIN) delay(1);       //等 IR 变为低电平，跳过4.5ms的前导高电平信号。
 			while(!IRIN) delay(1);         //等 IR 变为高电平
			while(IRIN){           //计算IR高电平时长
				delay(1);
				N++;           
				if(N>=30){
					EX0 = 1;
	 				return;
				}                  //0.14ms计数过长自动离开。
			}                        //高电平计数完毕                
			IRCOM[j] = IRCOM[j] >> 1;                  //数据最高位补“0”
			if (N>=8) IRCOM[j] = IRCOM[j] | 0x80;  //数据最高位补“1”
			N = 0;
		}//end for k
	}//end for j
	if (IRCOM[2]!=~IRCOM[3]){
		EX0 = 1;
		return;
	}

	IRCOM[5] = IRCOM[2] & 0x0F;     //取键码的低四位
	IRCOM[6] = IRCOM[2] >> 4;       //右移4次，高四位变为低四位
 
   	set1 = IRCOM[6];
	set2 = IRCOM[5];
	flag_number = 0;
	if(set1==0){
		if(set2==12) {number = 1;flag_number = 1;}
		else if(set2==8) {number = 4;flag_number = 1;}
	}
	else if(set1==1){
		switch (set2){
			case 6: number = 0;flag_number = 1;break;
			case 12: number = 5;flag_number = 1;break;
			case 8: number = 2;flag_number = 1;break;
			default:break;
		}
	}
	else if(set1==4){
		switch (set2){
			case 2: number = 7;flag_number = 1;;break;
			case 10: number = 9;flag_number = 1;break;
			case 4: senddata = 1;break;
			case 7: flag_keybeep = ~flag_keybeep;break;
			default: break;
		}
	}		
    else if(set1==5){
		if(set2==2) {number = 8;flag_number = 1;}
		else if(set2==10) {number = 6;flag_number = 1;}
		else if(set2==14) {number = 3;flag_number = 1;}
	}
	if(flag_keybeep)
		beep();
	EX0 = 1; 
} 
 
