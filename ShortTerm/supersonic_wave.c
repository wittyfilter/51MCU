#include <reg52.h>
#include <intrins.h>
#define uint  unsigned int
#define uchar unsigned char
			
sfr  io = 0xA0;		//P0-0x80,P1-0x90,P2-0xA0,P3-0xB0;
sbit rs = P0^7;		//LCD数据/命令选择端(H/L)
sbit rw = P0^6;		//LCD读/写选择端(H/L)
sbit ep = P0^5;		//LCD使能控制
sbit bz = io^7;		//LCD忙标志位
sbit trig = P3^3;	//触发控制信号输入
sbit echo = P3^2;	//回响信号输出
sbit beep = P0^4;   //蜂鸣器
uchar display[] = {"000 cm"};//距离显示
long t, distance, sum, i;

/*------------子函数定义---------------*/
//延时
void delay(uint ms)
{
	uint t;
	while(ms--)
		for(t=0;t<120;t++);
}	

void NOP(void)
{
	_nop_();_nop_();_nop_();_nop_();
	_nop_();_nop_();_nop_();_nop_();
	_nop_();_nop_();_nop_();_nop_();
	_nop_();_nop_();
}

//测试LCD忙碌状态
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

//写入指令到LCD
void lcd_wcmd(uchar cmd)
{							
	lcd_busy();	//检测忙
	rs = 0;		//指令
	rw = 0;		//写入
	ep = 1;	
	io = cmd;	//指令
	ep = 0;		//下降沿有效	
}

//写入数据函数
void lcd_wdat(uchar Data)
{
	lcd_busy();  //检测忙
	rs = 1;		 //数据
	rw = 0;		 //写入
	ep = 1;
	io = Data;	 //数据
	ep = 0;		 //下降沿有效
}

//LCD数据指针位置程序
void lcd_pos(uchar x, bit y)
{						
	if(y)lcd_wcmd(x|0xc0);	//y=1,第二行显示;y=0,第一行显示		0<=x<16
	else lcd_wcmd(x|0x80);	//数据指针=80+地址码(00H~27H,40H~67H)
}

//显示字符串
void prints(uchar *string)
{
	while(*string) {lcd_wdat(*string);string++;}
}

//LCD初始化
void lcd_init()
{						
	lcd_wcmd(0x38);		//设置LCD为16X2显示,5X7点阵,八位数据接口
	lcd_wcmd(0x06);		//LCD显示光标移动设置(光标地址指针加1,整屏显示不移动)
	lcd_wcmd(0x0c);		//LCD开显示及光标设置(光标不闪烁,不显示"_")
	lcd_wcmd(0x01);		//清除LCD的显示内容
}

//超声波模块初始化
void HC_Init()
{
	trig = 1;					//触发脉冲
	NOP();
	trig = 0;
	distance = 0.19*t;			//距离计算
}

//数字转字符
void data2char(long dat)
{
	display[0] = dat/1000 + '0';
	display[1] = dat/100%10 + '0';
	display[2] = dat/10%10 + '0';
}

//报警蜂鸣 
void alarm(void)
{
	for(i=0;i<255;i++){
		beep = ~beep;
		delay(1);
	}
}
		
/*------------主程序-----------*/
void main()
{
	t = 1000;
	lcd_init();
	delay(5);
	lcd_pos(0,0);
	prints("Distance:");
	TMOD = 0x19;
	EA = 1;			//开总中断
	TR0 = 1;		//启动定时器
	EX0 = 1;		//开外部中断
	IT0 = 1;		//设置为下降沿中断方式

	while(1){
		sum=0;
		for(i=0;i<1000;i++){
		HC_Init();
		sum = sum + distance;
		}
		distance = sum/1000;		//采样1000次取平均 
		if((distance<50) && (distance>0))	//若小于5cm报警 
			alarm();
		else 
			beep = 1;
		data2char(distance);
		lcd_pos(0,1);
		prints(display);
	}
}

//中断函数
void int0() interrupt 0
{
	t = TH0*256 + TL0;	//计数作为距离参量 
	TH0 = 0;
	TL0 = 0;
}
