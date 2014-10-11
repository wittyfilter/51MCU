#include <reg52.h>
#include <math.h>
#include <intrins.h>
#define uchar unsigned char
#define lint long int
#define uint unsigned int

//---------	AD9851控制IO定义------------------------
sbit load = P3^4;      	 	//AD9851信号更新位FQ_UP
sbit clk = P3^6;      	 	//写控制字时钟WCLK	 
sbit dat = P3^7; 				//数据位D7	
sbit reset = P3^5;
sbit clk2 = P0^0;
sbit A1 = P1^4;
sbit B1 = P1^3;
sbit C1 = P1^2;
sbit A2 = P1^7;
sbit B2 = P1^6;
sbit C2 = P1^5;
sbit key1 = P3^2;		//按键 
sbit key2 = P3^3;
sbit rs = P0^7;			//数据/命令 
sbit rw = P0^6;			//读/写 
sbit e = P0^5;			//使能 

/*------------------------与AD有关特殊功能寄存器定义------------------------------*/
sfr  P1ASF = 0x9D;
sfr  ADC_CONTR = 0xBC;
sfr  ADC_RES = 0xBD;
sfr  ADC_RESL = 0xBE;
sfr  AUXR1 = 0xA2;

/*-------------------------------ADC功能设置定义-------------------------------------*/
#define ADC_OFF ADC_COUNTER=0
#define	ADC_ON 		(1 << 7)	//AD上电
#define ADC_90T		(3 << 5)	//AD转换速度选择
#define ADC_180T	(2 << 5)
#define ADC_360T	(1 << 5)
#define ADC_540T	0
#define ADC_CLRFLAG	239			//AD转换完成标志位软件清0
#define ADC_START   (1 << 3)	//AD转换使能，自动清0
#define ADC_SELEC_CHL 0x06		//P1.0、P1.1口作为模拟功能AD使用
#define ADC_CH0		0			//通道选择 0~7
#define ADC_CH1		0x01
#define ADC_CH2		0x02
#define ADC_CH3		0x03
#define ADC_CH4		0x04
#define ADC_CH5		0x05
#define ADC_CH6		0x06
#define ADC_CH7		0x07
#define ADC_MOD0	0			//转换结果储存模式0: ADC_RES[7:0]+ADC_RES[1:0]

#define	ADC_MOD1	(1 << 2)	//转换结果储存模式1: ADC_RES[1:0]+ADC_RES[7:0]

int adc_result1 = 0 , adc_result2 = 0;		//AD转换结果储存器
uchar i, j, k, m = 0; 
bit flag = 1;

//////////////////////////////////////////////////////////////
///-----打开STC12C5A60AD AD转换器的正确姿势-----------------//
///*1.P1ASF^1 / P1ASF^2 = 1; 	通道使能					//
///*2.ADC_POWER=1;	  ADC上电								//
///*3.SPEED0=1, SPEED1=1;									//
///*4.CHS2=0,CHS1=0,CHS0=1;  选择一个通道 					//
///*5.ADC_START=1;											//
///*6.delay;	适当延时									//
///*7.ADRJ=1;												//
///*8.uint result={"000000",ADC_RES[1:0],ADC_RESL[7:0]};	//
//////////////////////////////////////////////////////////////

//------------------------ADC转换及处理函数------------------------//
void ADC(uint channel)
{		
		float result1 = 0 , result2 = 0;	//将AD转换结果转化成比例数值

	 	if(channel==1)				//选择通道1
			ADC_CONTR |= ADC_CH1;
	
		if(channel==2)				//选择通道2
			ADC_CONTR |= ADC_CH2;
			 	
		ADC_CONTR |= ADC_START;		//开始转换
	
		while((ADC_CONTR&0x10)!=0x10);	//等待转换完成
		ADC_CONTR &= 0xEF;			//软件清除标志位

	    if(channel==1){
			adc_result1 = ADC_RES;		
			adc_result1	= (adc_result1<<8) | ADC_RESL;	//储存转换结果
			result1=((float)adc_result1/1024)*5;  //转化成相对于5.00V 的电平值
			ADC_CONTR &= 0xF8;			  //清除通道选择		
		}
		if(channel==2){
		  	adc_result2 = ADC_RES;		
			adc_result2	= (adc_result2<<8) | ADC_RESL;	//储存转换结果
			result2=((float)adc_result2/1024)*5;  //转化成相对于5.00V 的电平值		
			ADC_CONTR &= 0xF8;			  //清除通道选择	
		}
		 
}
 /*--------------------延时函数-------------------------*/
void delay_50us(uint x)			
{
    uchar i,j;
	for(i=0;i<x;i++)
		for(j=0;j<10;j++);
}

/*--------------------液晶函数--------------------------*/
//忙信号检测
void testbusy(void)			 
{
    do{rs = 0;
	rw = 1;
	e = 1;
	P2 = 0xff;
	_nop_();}
	while((P2&0x80)==0x80);
	e = 0;
}

 //写指令
void send_com(uchar com_data)	  
{
    testbusy();
	rs = 0;
	rw = 0;
	e = 0;
	P2 = com_data; 	
	delay_50us(5);
	e = 1;	
	delay_50us(5);
	e = 0;			  
}

//写数据
void send_data(uchar tab_data)	  
{
	rs = 1;
	rw = 0;
	e = 0;
	P2 = tab_data;
	delay_50us(5);
	e = 1; 	
	delay_50us(5);
	e = 0;
}

 //初始化函数	  
void init(void)			
{
    delay_50us(10);
	send_com(0x30);
	send_com(0x30);
	send_com(0x0c);
	send_com(0x01);
	send_com(0x06);
	delay_50us(1);
}

//清除GDRAM
void clrgdram(void)		   
{
    uchar x,y;
	for(y=0;y<64;y++)		 //64行
	for(x=0;x<16;x++){		 //16列
	    send_com(0x34);
		send_com(y+0x80);	 //行地址
		send_com(x+0x80);	 //列地址
		send_com(0x30);
		send_data(0x00);
		send_data(0x00);
	}
}

uchar readbyte(void)
{
    uchar byreturnvalue;
	testbusy();
	P2 = 0xff;
	rs = 1;
	rw  =1;
	e = 0;
	e = 1;
	byreturnvalue = P2;
	e = 0;
	return byreturnvalue;
}

//画点函数
void drawpoint(uchar x, uchar y, uchar color)	  
{
    uchar row, tier, tier_bit;
	uchar readH, readL;
	send_com(0x34);
	send_com(0x36);
	tier = x>>4;
	tier_bit = x&0x0f;
	if(y<32){
	    row = y;
	}
	else{
	    row = y-32;
		tier += 8;
	}
	send_com(row+0x80);
	send_com(tier+0x80);
	readbyte();
	readH = readbyte();
	readL = readbyte();
	send_com(row+0x80);
	send_com(tier+0x80);
	if(tier_bit<8){
	    switch(color){
		    case 0: readH&=(~(0x01<<(7-tier_bit)));
			break;
			case 1: readH|=(0x01<<(7-tier_bit));
			break;
			case 2: readH^=(0x01<<(7-tier_bit));
			break;
			default:
			break;
		}
		send_data(readH);
		send_data(readL);
	}
	else{
	    switch(color){
		    case 0: readL&=(~(0x01<<(15-tier_bit)));
			break;
			case 1: readL|=(0x01<<(15-tier_bit));
			break;
			case 2: readL^=(0x01<<(15-tier_bit));
			break;
			default:
			break;
		}
		send_data(readH);
		send_data(readL);
	}
	send_com(0x30);
}

//显示字符串 
void prints(uchar code*s)			
{
    while(*s>0){
        send_data(*s);
        s++;
        delay_50us(50);
    }
}

/*--------------------------------------------------------------*/
//延时函数定义
void delayms(uchar count){		//延迟函数，参数为毫秒数
	uchar i,j;
	for(i=0;i<count;i++)
		for(j=0;j<240;j++); 
} 

				
void delay(uint t)
{
	uint i;
	while(t--)
	{
		/* 对于12M时钟，约延时1ms */
		for (i=0;i<125;i++)
		{}
	}
}

void ad9850_reset() //初始化 
{ 
	clk = 0; 
	load = 0; 
	reset = 0;

	reset = 1;
	reset = 0;

	clk = 1; 
	clk = 0; 

	load = 1; 
	load = 0; 
} 

					
void write_dds(unsigned long dds)
{
    uchar temp = 0x80;
	uchar i;
	load = 0;
	clk = 0;
	for(i=0;i<40;i++){
		clk = 0;
		delay(1);
		if(dds&0x00000001){
			dat = 1;
		}
		else dat = 0;
		delay(1);
		clk = 1;
		dds = dds>>1;
	}
	load = 1;
	clk = 0;
	delay(1);
	load = 0;
}

 void write_freq(unsigned long frequency)
{
	frequency = (unsigned long)(34.35973837*frequency);    //使用125M晶振,frequence就是所要输出的频率
	write_dds(frequency);
}

void main()
{	
	uchar i, colour = 1;
	uchar temp[90];
	unsigned long freq = 0; 
	init();
	clrgdram();
	send_com(0x01);
	delay_50us(10); 
	ad9850_reset();
	P1ASF = ADC_SELEC_CHL;				//选择P1.1和P1.2作为ADC输入口,通道1对应AD1，通道2对应AD2
	ADC_CONTR &= 0;						//先对ADC_CONTR初始化
	ADC_CONTR = ADC_ON | ADC_90T ;		//ADC上电并且速度设为90倍时钟周期 T					
	AUXR1 |= ADC_MOD1;				    //转换结果存储模式为ADC_RES[1:0]+ADC_RESL[7:0]
	send_com(0x80);				
    prints("Amp:");
	send_com(0x82);
    prints("00");
	send_com(0x83);
	prints("dB");
	A1 = 0;B1 = 0;C1 = 0;
	A2 = 0;B2 = 0;C2 = 0;
	EA = 1;         //允许总中断
	TMOD = 0x02;
	TH0 = 0xFF;
	TL0 = 0xFF;
	ET0 = 1;
	TR0 = 1;	 
	EX0 = 1;
	IT0 = 1;		//设置为下降沿中断方式
	EX1 = 1;
	IT1 = 1;		//设置为下降沿中断方式
	while(1){	
		if(flag){
			clrgdram();	 
			for(i=0;i<120;i++)	  			//画坐标
				drawpoint(i,63,colour);
			for(i=0;i<54;i++)
				drawpoint(0,64-i,colour);
			for(i=0;i<90;i++){				//扫频并获取有效值
				write_freq(freq);
				freq = (freq + 25000)%2250000;
				if(k<5){						
					ADC(1);
					temp[i] = adc_result1;}
			}		
			for(i=4;i<94;i++)				//画点
				drawpoint(i,64-temp[i-4],colour);
			flag = 0;	
		}
	}
}

void int0() interrupt 0
{
	delayms(5);
	if(!key1){
		k= (k+1)%7;
		flag = 1;
		while(!key1);
		delayms(5);
		while(!key1);
	}
	switch(k){
		case 0:
			A1=0;B1=0;C1=0;
			A2=0;B2=0;C2=0;break;
		case 1:
			A1=1;B1=0;C1=0;	
			A2=0;B2=0;C2=0;break;
		case 2:
			A1=0;B1=1;C1=0;
			A2=0;B2=0;C2=0;break;
		case 3:
			A1=1;B1=1;C1=0;
			A2=0;B2=0;C2=0;break;
		case 4:
			A1=0;B1=0;C1=1;
			A2=0;B2=0;C2=0;break;
		case 5:
			A1=0;B1=1;C1=0;
			A2=1;B2=1;C2=0;break;
		case 6:
			A1=0;B1=1;C1=0;
			A2=0;B2=0;C2=1;break;
	}
	send_com(0x82);
	send_data(k+'0');
   	send_data('0');
}

void timer0() interrupt 1
{
	clk2 = ~clk2;
	for(i=0;i<j;i++);
}
 
void int1() interrupt 2
{
	delayms(5);
	if(!key2){
		m = (m+1)%9;
		flag = 1;
		while(!key2);
		delayms(5);
		while(!key2);
	}
	switch(m){
		case 0:j=44;break;
		case 1:j=20;break;
		case 2:j=12;break;
		case 3:j=8;break;
		case 4:j=6;break;
		case 5:j=4;break;
		case 6:j=3;break;
		case 7:j=2;break;
		case 8:j=1;break;
		default:break;
	}			
} 