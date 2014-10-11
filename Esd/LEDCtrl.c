#include <reg51.h>
#include <intrins.h>
#define uchar unsigned char
#define uint unsigned int
			
//定义键位
sbit K1 = P1^0;
sbit K2 = P1^1;
//定义全局变量	
uchar i, state, LED, low, high, keyon;
bit flag;

/*-----------------------------------------------*/
/*				子函数部分						 */
/*-----------------------------------------------*/

//定义5ms空操作延时函数，用于按键去抖动
void delay5ms()
{
	uchar i,j;
	for(i=0;i<5;i++)
		for(j=0;j<240;j++); 
}
 
//定义精确延时函数，延时时间为 10*x ms
void delay(uint x)
{	
	uint time;
	//初始化计数变量i及清除延时结束标志
	i = 0;
	flag = 0;
	//计算定时器初值
	time = 65536 - 100*x;
	low = time%16 + 16*((time/16)%16);
	high = (time/256)%16 + 16*((time/4096)%16);
	TMOD = 0x01;	//设置T0定时模式1
	TL0 = low; 		//设置T0低8位初值
	TH0 = high;		//设置T0高8位初值
	ET0 = 1;		//定时器T0中断允许
	TR0 = 1;		//定时器T0启动	
}

//定义按键检测函数，按键状态体现为变量keyon值
//0为无按下，1为按下K1，2为按下K2(包含去抖动判断)
void Keyscan()
{
	if(!K1){
		delay5ms();
		if(!K1){
			keyon = 1;
			while(!K1);
			delay5ms();
			while(!K1);
		}
	}
	else if(!K2){
		delay5ms();
		if(!K2){
			keyon = 2;
			while(!K2);
			delay5ms();
			while(!K2);
		}
	}
	else keyon = 0;
}

//模式1：亮两灯，右移，间隔1秒	
void mode1()
{
	LED = 0xfc;	//0xfc = 1111 1100
	P0 = LED;
	flag = 1;		      
	Keyscan();
	while(keyon != 1){
		Keyscan();
		if(!flag)continue;
		P0 = LED;
		delay(100);
		LED = _cror_(LED,1);		
	}
}

//模式2：亮一灯，左移，间隔0.5秒	
void mode2()
{
	LED = 0xfe;	//0xfe = 1111 1110
	P0 = LED;
	flag = 1;
	Keyscan();
 	while(keyon != 1){
		Keyscan();
		if(!flag)continue; 
    	P0 = LED;
		delay(50);
		LED = _crol_(LED,1);		
	}
}

//模式3：亮两灯，右移，手动	
void mode3()
{
	LED = 0xfc;
	P0 = LED;	  
 	while(1){
		Keyscan();
		if(keyon == 1)
			return;
		else if(keyon == 2){
			LED = _cror_(LED,1);
			P0 = LED;
		}
	}				
}

//模式4：亮一灯，左移，手动	
void mode4()
{
	LED = 0xfe;
	P0 = LED;	  
 	while(1){
		Keyscan();
		if(keyon == 1)
			return;
		else if(keyon == 2){
			LED = _crol_(LED,1);
			P0 = LED;
		}
	}
}	

/*-----------------------------------------------*/
/*				程序主函数						 */
/*-----------------------------------------------*/

int main()
{
	i = 0;
	EA = 1;	//开总中断
	state = 3;
	while(1){	
		state = (state + 1) % 4;	//模式循环切换    	
		switch(state){				//四种模式
			case 0: mode1(); break;
			case 1: mode2(); break;
			case 2: mode3(); break;
			case 3: mode4(); break;
			default: break;
		}
	}
}

/*-----------------------------------------------*/
/*				中断函数						 */
/*-----------------------------------------------*/

//定时器T0溢出中断
void timer0_int(void) interrupt 1
{
	TL0 = low;
	TH0 = high;
	//若延时结束，置标志位flag，清除计数i
	if(++i == 100){
		i = 0;
		flag = 1;	
	}
}


