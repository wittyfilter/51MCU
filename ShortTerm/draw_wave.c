#include <reg52.h>
#include <math.h>
#define uchar unsigned char
#define uint unsigned int

sbit rs = P0^7;			//数据/命令 
sbit rw = P0^6;			//读/写 
sbit e = P0^5;			//使能 
sbit key1 = P3^5;		//按键 
sbit key2 = P3^4;
sbit key3 = P3^3;
sbit key4 = P3^2;

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
    rs = 0;
	rw = 1;
	e = 1;
	P2 = 0xff;
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

/*--------------------主函数--------------------------*/
void main(void)
{
    uchar i,j,colour=1;
	uchar m=10,n=30;
	init();
	clrgdram();
	send_com(0x01);
	delay_50us(10);
	
	while(1){
		//按键检测 
		for(i=4;i<124;i++){
		    if(!key1){
			    m+=5;			 //增大幅值
				while(!key1);
				clrgdram();
			}
			if(!key2){
			    m-=5;			 //减小幅值
				while(!key2);
				clrgdram();
			}
			if(!key3){
			    n+=5;			 //增大周期
				while(!key3);
				clrgdram();
			}
			if(!key4){
			    n-=5;			 //减小周期
				while(!key4);
				clrgdram();
			}
			
			j = 35 - m*sin((i-4)*3.14/n);
			drawpoint(i,j,colour);
		}
	
	send_com(0x80);				//显示周期 
    prints("T:");
   	send_com(0x81);
    send_data(n/10+'0');
    send_data(n%10+'0');
	send_com(0x83);				//显示幅值 
	prints("A:");
   	send_com(0x84);
    send_data(m/10+'0');
    send_data(m%10+'0');
	}
}
