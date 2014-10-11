#include <reg52.h>
#include <I2C.H>
#include <intrins.h>
#define  PCF8591 0x90	//PCF8591 地址
#define uint unsigned int
#define uchar unsigned char

extern GetTemp();							//声明引用外部函数
extern unsigned int  idata Temperatureint;		//声明引用外部变量
extern unsigned char  idata Temperaturefloat;	//温度的整数部分与小数部分

sbit K1 = P1^0;
uchar mode;

/*--------------------------------------------------------------*/
/*                     LCD函数部分 								×/
/*--------------------------------------------------------------*/

//LCD接口定义					
sfr		 io	= 0x80;				//P0-0x80,P1-0x90,P2-0xA0,P3-0xB0;
sbit	 rs = P2^6;				//LCD数据/命令选择端(H/L)
sbit	 rw = P2^5;				//LCD读/写选择端(H/L)
sbit	 ep = P2^7;				//LCD使能控制
sbit     bz = io^7;				//LCD忙标志位

//函数声明		
void lcd_busy(void);			//测试LCD忙碌状态程序
void lcd_wcmd(uchar cmd);		//写入指令到LCD程序
void lcd_wdat(uchar dat);		//写入数据到LCD程序
void lcd_pos (uchar x, bit y);	//LCD数据指针位置程序
void lcd_init(void);			//LCD初始化设定程序

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

//LCD数据指针位置程序
void lcd_pos(uchar x, bit y)
{						
	if(y)lcd_wcmd(x|0xc0);	//y=1,第二行显示;y=0,第一行显示		0<=x<16
	else lcd_wcmd(x|0x80);	//数据指针=80+地址码(00H~27H,40H~67H)
}

//写入数据函数
void lcd_wdat(unsigned char Data)
{
	lcd_busy();  //检测忙
	rs = 1;		 //数据
	rw = 0;		 //写入
	ep = 1;
	io = Data;	 //数据
	ep = 0;		 //下降沿有效
}

//显示字符串
void prints(uchar *string)
{
	while(*string) {lcd_wdat(*string);string++;}
}

//LCD初始化设定	
void lcd_init()
{						
	lcd_wcmd(0x38);		//设置LCD为16X2显示,5X7点阵,八位数据接口
	lcd_wcmd(0x06);		//LCD显示光标移动设置(光标地址指针加1,整屏显示不移动)
	lcd_wcmd(0x0c);		//LCD开显示及光标设置(光标不闪烁,不显示"_")
	lcd_wcmd(0x01);		//清除LCD的显示内容
}


		
/*----------------------------------------------------------------*/
/*               				ADC函数               			  */
/*----------------------------------------------------------------*/

//发送字节
bit ISendByte(uchar sla, uchar c)
{
	Start_I2c();              //启动总线
	SendByte(sla);            //发送器件地址
	if(ack==0) return 0;
	SendByte(c);              //发送数据
	if(ack==0) return 0;
	Stop_I2c();               //结束总线
	return 1;
}

//读字节
uchar IRcvByte(uchar sla)
{  
	uchar c;
	Start_I2c();          //启动总线
	SendByte(sla+1);      //发送器件地址
	if(ack==0) return 0;
	c=RcvByte();          //读取数据0	
	Ack_I2c(1);           //发送非就答位
	Stop_I2c();           //结束总线
	return c;
}  

/*----------------------------------------------------------------*/
/*               				数据显示              			  */
/*----------------------------------------------------------------*/

//温度
void displaytemp(uint x, uchar y)
{
	uchar tempchar[5];
	tempchar[1] = x%10+'0';
	tempchar[0] = (x/10)%10 + '0';
	tempchar[2] = '.';
	tempchar[3] = y + '0';
	tempchar[4] = '\0';
   	lcd_pos(0,1);
	prints("Temp:   ");
	lcd_pos(9,1);
	prints(tempchar);
	lcd_pos(13,1);
	prints("    ");	
}

//电压
void displayvoltage(uint x)
{
	uchar volchar[5];
	volchar[3] = x%10+'0';
	volchar[2] = (x/10)%10 + '0';
	volchar[1] = '.';
	volchar[0] = (x/100)%10 + '0';
	volchar[4] = '\0';
   	lcd_pos(0,1);
	prints("Voltage:");
	lcd_pos(9,1);
	prints(volchar);
	lcd_pos(13,1);
	prints("    ");	
}

//光照
void displaylight(uint x)
{
	uchar lightchar[2];
	if (((x/100)%10) > 4)
		lightchar[0] = 'A';
	else if (((x/100)%10) > 3)
		lightchar[0] = 'B';
	else if (((x/100)%10) > 2)
		lightchar[0] = 'C';
	else if (((x/100)%10) > 1)
		lightchar[0] = 'D';
	else 
		lightchar[0] = 'E';
	lightchar[1] = '\0';
   	lcd_pos(0,1);
	prints("Intense:");
	lcd_pos(9,1);
	prints(lightchar);
	lcd_pos(10,1);
	prints("       ");	
}

/*----------------------------------------------------------------*/
/*               			模式子函数               			  */
/*----------------------------------------------------------------*/

//模式1：获取温度
void mode1()
{
	GetTemp();
	displaytemp(Temperatureint,Temperaturefloat);
}

//模式2：获取电压
void mode2()
{
	uint vol;
	ISendByte(PCF8591,0x41);
	vol = IRcvByte(PCF8591)*2;		//模数转换口1
	displayvoltage(vol);
}

//模式3：获取光照强度
void mode3()
{
	uint light;
	ISendByte(PCF8591,0x43);
	light = IRcvByte(PCF8591)*2;	//模数转换口3
 	displaylight(light);
}

/*----------------------------------------------------------------*/
/*               			程序主函数               			  */
/*----------------------------------------------------------------*/

//延时函数定义
void delayms(uchar count){		//延迟函数，参数为毫秒数
	uchar i,j;
	for(i=0;i<count;i++)
		for(j=0;j<240;j++); 
} 

//主函数
int main()
{
	//定时器初始化，用于DS18B20	
	TMOD|= 0x11;
    TH1 = 0xD8;
    TL1 = 0xF0;	
	IE = 0x8A;	
    TR1  = 1; 
	lcd_init();
	lcd_pos(0,0); 
	prints("Mode:");
	while(1){
		if(!K1){
			delayms(5);
			if(!K1){
				mode = (mode + 1) % 3;
				while(!K1);
				delayms(5);
				while(!K1);
			}
		}
		switch(mode){		//三种模式切换
			case 0: lcd_pos(6,0);prints("Temp   ");mode1();break;
			case 1: lcd_pos(6,0);prints("Voltage");mode2();break;
			case 2: lcd_pos(6,0);prints("Light  ");mode3();break;
			default:break;
		}
	} 
}
