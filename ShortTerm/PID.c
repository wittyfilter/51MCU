#include <>reg52.h>	  	// 使用原来的reg52.h头文件，特殊功能寄存器自己单独定义
#include <string.h>
#include <intrins.h>
#define uint unsigned int
#define uchar unsigned char

/*------------------------与AD有关特殊功能寄存器定义-------------------------*/
sfr  P1ASF = 0x9D;
sfr  ADC_CONTR = 0xBC;
sfr  ADC_RES = 0xBD;
sfr  ADC_RESL = 0xBE;
sfr  AUXR1 = 0xA2;

/*----------------------------ADC功能设置定义--------------------------------*/
#define ADC_OFF ADC_COUNTER=0
#define	ADC_ON 		(1 << 7)	//AD上电
#define ADC_90T		(3 << 5)	//AD转换速度选择
#define ADC_180T	(2 << 5)
#define ADC_360T	(1 << 5)
#define ADC_540T	0
#define ADC_CLRFLAG	239			//AD转换完成标志位软件清0
#define ADC_START   (1 << 3)	//AD转换使能，自动清0
#define ADC_SELEC_CHL 0x06		//P1.1、P1.2口作为模拟功能AD使用
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

/*-------------------------------DAC2904功能设置定义-------------------------------------*/
sbit PD = P0^0;		//节能模式
sbit GSET = P0^1;	//增益设置模式，可设置为两路DAC同步调节或分别调节
sbit WRT1 = P0^2;	//DAC1写使能信号
sbit CLK1 = P0^3;	//DAC1时钟

sbit Dac13 = P3^5;	//DAC输出接口
sbit Dac12 = P3^4;
sbit Dac11 = P3^3;
sbit Dac10 = P3^2;
sbit Dac9 = P3^1;
sbit Dac8 = P3^0;
sbit Dac7 = P2^7;
sbit Dac6 = P2^6;
sbit Dac5 = P2^5;
sbit Dac4 = P2^4;
sbit Dac3 = P2^3;
sbit Dac2 = P2^2;
sbit Dac1 = P2^1;
sbit Dac0 = P2^0;

int adc_result1 = 0 , adc_result2 = 0;		//AD转换结果储存器
int dac_out=0;								//DAC输出，DAC2904为14 bit

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

//---------------------------PID结构体定义--------------------------//
typedef	struct	PID
{
	int	Setpoint;		  	//设定目标值
	float	P;	      			//比例常数
	float	I;					//积分常数
	float	D;					//微分常数
	float	LastError;			//eror[-1]
	float	PrevError;			//error[-2]
} PID;

PID sPID;				//定义一个静态PID变量

//--------------------------LCD接口定义------------------------------//					
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
void display(float x,uint i)
{
	uchar datachar_int[3];		 //整数部分
	uchar datachar_res[3];		 //小数部分
	datachar_int[1] = (int)x%10+'0';
	datachar_int[0] = ((int)x/10)%10 + '0';
	datachar_int[2] = '\0';
		
	datachar_res[0] = ((int)(x*10))%10+'0';
	datachar_res[1] = ((int)(x*100))%10 + '0';
	datachar_res[2] = '\0';
   	lcd_pos(5,i);
	prints(datachar_int);		 //显示整数
	lcd_pos(7,i);
	prints(".");
	lcd_pos(8,i);
	prints(datachar_res);		 //显示小数
}

//-----------------------------延时函数----------------------------//
void delay(uint x)	   
{
	uint i,j;
	for(i=0;i<x;i++);
		for(j=0;j<10000;j++);
}

//------------------------ADC转换及处理函数------------------------//
void ADC(uint channel)
{		
	float result1 = 0, result2 = 0;	//将AD转换结果转化成比例数值

	if(channel==1){				//选择通道1
		ADC_CONTR |= ADC_CH1;
	}
	if(channel==2){				//选择通道2
		ADC_CONTR |= ADC_CH2;
	}
		
	ADC_CONTR |= ADC_START;		//开始转换

	while((ADC_CONTR&0x10)!=0x10);	//等待转换完成
	ADC_CONTR &= 0xEF;			//软件清除标志位

	if(channel==1){
		adc_result1 = ADC_RES;		
		adc_result1	= (adc_result1<<8) | ADC_RESL;	//储存转换结果
		result1 = ((float)adc_result1/1024)*5;  //转化成相对于5.00V 的电平值
		display(result1,0);		      //显示
		ADC_CONTR &= 0xF8;			  //清除通道选择	
	}
	if(channel==2){
		adc_result2 = ADC_RES;		
		adc_result2	= (adc_result2<<8) | ADC_RESL;	//储存转换结果
		result2 = ((float)adc_result2/1024)*5;  //转化成相对于5.00V 的电平值
		display(result2,1);		      //显示			
		ADC_CONTR &= 0xF8;			  //清除通道选择
	}	 
}

//----------------------------DAC转换及处理函数----------------------------//
void DAC()
{	
	int temp,i;
	temp = dac_out;
	for(i=0;i<14;i++){					//DAC输出
		switch(i){
		   case 0: if(temp%2) Dac0=1; else Dac0=0; break;
		   case 1: if(temp%2) Dac1=1; else Dac1=0; break;
		   case 2: if(temp%2) Dac2=1; else Dac2=0; break;
		   case 3: if(temp%2) Dac3=1; else Dac3=0; break;
		   case 4: if(temp%2) Dac4=1; else Dac4=0; break;
		   case 5: if(temp%2) Dac5=1; else Dac5=0; break;
		   case 6: if(temp%2) Dac6=1; else Dac6=0; break;
		   case 7: if(temp%2) Dac7=1; else Dac7=0; break;
		   case 8: if(temp%2) Dac8=1; else Dac8=0; break;
		   case 9: if(temp%2) Dac9=1; else Dac9=0; break;
		   case 10: if(temp%2) Dac10=1; else Dac10=0; break;
		   case 11: if(temp%2) Dac11=1; else Dac11=0; break;
		   case 12: if(temp%2) Dac12=1; else Dac12=0; break;
		   case 13: if(temp%2) Dac13=1; else Dac13=0; break;
		   default:break;
		}
		temp /= 2;
	}				
//	delay(500);					//延时
	WRT1 = 1;					//DAC写使能
	CLK1 = 1;					//DAC时钟高电平	 	
//	delay(1000);					//延时
	WRT1 = 0; 					//DAC写使能置零
	CLK1 = 0;					//DAC时钟低电平
//	delay(500);					//延时	
}

//----------------------------------PID初始化-------------------------------//
void IncPIDInit()
{
	sPID.Setpoint = 0;		  
	sPID.P = 0.3;	      		
	sPID.I = 0.5;				
	sPID.D = 0.5;					
	sPID.LastError = 0;		
	sPID.PrevError = 0;				
}			
	
//--------------------------------增量式PID算法-----------------------------//
int IncPID(int Nextpoint)
{
	int	iError = 0, iIncpid = 0;				//当前误差与增量	
    iError = sPID.Setpoint - Nextpoint;	 	//当前误差
    iIncpid = sPID.P*iError - sPID.I*sPID.LastError + sPID.D*sPID.PrevError; //增量计算
    sPID.PrevError = sPID.LastError;			//PrevError更新
    sPID.LastError = iError;					//LastError更新
    return(iIncpid);		  					//返回增量  	
}
  	
//------------------------------主函数------------------------------//
void main()
{	
	int Inc = 0;						//用于DAC输出的增量
	P1ASF = ADC_SELEC_CHL;				//选择P1.1和P1.2作为ADC输入口,通道1对应AD1，通道2对应AD2
	ADC_CONTR &= 0;						//先对ADC_CONTR初始化
	ADC_CONTR = ADC_ON | ADC_90T ;		//ADC上电并且速度设为90倍时钟周期 T					
	AUXR1 |= ADC_MOD1;				    //转换结果存储模式为ADC_RES[1:0]+ADC_RESL[7:0]
	
	lcd_init();
	lcd_pos(1,0);
	prints("AD1:");
	lcd_pos(1,1);
	prints("AD2:");
	lcd_pos(12,0);
	prints("V");
	lcd_pos(12,1);
	prints("V");	
	  
	IncPIDInit();
	GSET = 0;	  //双电阻调节增益模式
	PD = 0;		  //DAC2904关闭节能模式

	while(1){
		ADC(1);
		ADC(2);
		sPID.Setpoint = adc_result1;
		Inc = IncPID(adc_result2);
		dac_out = dac_out + Inc;			//DAC输出历史值加上增量
		DAC();							//DAC输出
	}
}
