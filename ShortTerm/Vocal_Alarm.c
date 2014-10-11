#include <reg52.h>
#include <intrins.h>
#define uchar unsigned char
#define uint unsigned int

sbit Data = P1^4;			//数据输出
sbit Cs = P1^5;			//片选信号
sbit Clk = P1^6;			//WT588D的时钟信号

sbit alarm_key = P3^5;		//报时按键
sbit vol_set_key = P3^4;		//音量调节按键，加到最大后跳到最小值
sbit time_adj_key = P3^3;		//时间调节按键
sbit time_selc_key = P3^2;	//时间调节位选择

uchar vol = 0xE5;			//音量值，范围[E0H,E7H]
uchar addr = 0x00; 				//语音播放地址，直接用来触发播放该地址语音信号
static uint miao = 0, fen = 0, shi = 0;	//时间变量
uint t0_count = 0;			//定时器中断计数
uint flag = 0;				//时间更改位选择标志位
uint gao = 0, di = 0;			//时间更改临时变量

/*--------------------------------------------------------------*/
/*                     LCD函数部分 								×/
/*--------------------------------------------------------------*/

//------------------------LCD接口定义---------------------------//					
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
void display(uint x,uint y,uint t)
{
	uchar datachar[3];
	datachar[1] = t%10+'0';
	datachar[0] = (t/10)%10 + '0';
	datachar[2] = '\0';
   	lcd_pos(x,y);
	prints(datachar);	
}

//--------------------------显示当前时间-----------------------//
void display_time()
{  	
	display(4,1,shi);
	display(7,1,fen);
	display(10,1,miao);
}

/*--------------------------------------------------------------*/
/*                     时钟调节函数部分 						*/
/*--------------------------------------------------------------*/
//--------------------------秒调节函数--------------------------//
void miao_inc()
{
	miao++;
	if(miao>59){		//秒进位
		miao = 0;
		fen++;
		if(fen>59){		//分进位
			fen = 0;
			shi++;
			if(shi>23){	//时归零
				shi = 0;
			}
		}
	}
}

//--------------------------分调节函数--------------------------//
void fen_inc()
{
	
	fen++;
	if(fen>59){		//分进位
		fen = 0;
		miao = 0;
		shi++;
		if(shi>23){	//时归零
			shi = 0;
		}
	}	
}

//--------------------------时调节函数--------------------------//
void shi_inc()
{
	shi++;
	if(shi>23){	//时归零
		shi = 0;
		fen = 0;
		miao = 0;
	}
}


//----------------------延时k个50us子程序---------------------//
void delay50us(uint k)		
{
	uint i,j;
	for(j=0;j<k;j++)
		for(i=0;i<25;i++);
}

//----------------------三线控制子程序----------------------//
void send_threelines(uchar addr) 
{ 
	uint i;
	Cs = 0; 
 	delay50us(100);    /* 选号 电 片 信 保持低 平 5ms */ 

 	for(i=0;i<8;i++){ 
	 	Clk = 0; 
	 	if(addr & 1)	
			Data = 1; 
		else 	
			Data = 0; 
	 	addr>>=1; 
	 	delay50us(3);    /*半个时钟周期 150us */ 
	
		Clk = 1; 
	 	delay50us(3); 
	} 
	Cs = 1;			 
}

//----------------------音量调节子函数-----------------------//
void vol_adj(uchar set_vol)
{
	send_threelines(set_vol);
}

//-------------------语音播报当前时间-----------------------//
void alarm_now(uint shi_1,uint fen_1)
{
	uint gao, di;
	send_threelines(0X0C);		  //先播放提示铃声
	delay50us(3000);

	di = shi_1%10;				  //判断"时"的低位 
	gao = shi_1/10;			  	  //判断"时"的高位
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
	display_time();				  //软件方法避免冲突，原理不明
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////	
	if(gao==0){		  			  	  
		send_threelines(di); 		   //高位为0时只播放低位
	}
	else{							  
		if(gao==1){					  
			if(di==0){
				send_threelines(0x0A); //高位为1且低位为0时只播"十"
			}
			else{
				send_threelines(0x0A); //高位为1且低位不为0时播"十"和低位
				delay50us(2000);
				send_threelines(di);
			}
		}
		else if(di==0){				   //高位不为0不为1，且低位为0时播放高位和"十"
			send_threelines(gao);	  
			delay50us(2000);
			send_threelines(0x0A);	   //"十"
		}
		else{
			send_threelines(gao);	   //高位不为0不为1，且低位不为0时播放高位、"十"和低位
			delay50us(2000);
			send_threelines(0x0A);	   //"十"
			delay50us(2000);
			send_threelines(di);
		}
	}

	delay50us(2000);			  //延时200ms
	send_threelines(0x0B);		  //"点"
	delay50us(2000);

	di = fen_1%10;					  //判断"分"的高位
	gao = (fen_1/10)%10;			  //判断"分"的低位
	if(gao==0){		  			  //高位为0时只播放低位
		send_threelines(di); 		
	}
	else{
		if(gao==1){
			if(di==0){
				send_threelines(0x0A);
			}
			else{
				send_threelines(0x0A);
				delay50us(2000);
				send_threelines(di);
			}
		}
		else if(di==0){
			send_threelines(gao);	  //高位不为0且低位为0时播放高位和"十"
			delay50us(2000);
			send_threelines(0x0A);	  //"十"
		}
		else{
			send_threelines(gao);	  //高位不为0且低位不为0时播放高位、"十"和低位
			delay50us(2000);
			send_threelines(0x0A);	  //"十"
			delay50us(2000);
			send_threelines(di);
		}
	}
	
	delay50us(2000);
	send_threelines(0x0D);		  //"分"
}

//------------------------整点报时--------------------------//
void alarm_int()
{
	switch(shi){
		case 0:	 	 send_threelines(0x0E);	break;
		case 1:	 	 send_threelines(0x0F);	break;
		case 2:		 send_threelines(0x10);	break;
		case 3:		 send_threelines(0x11);	break;
		case 4:	 	 send_threelines(0x12);	break;
		case 5:	  	 send_threelines(0x13);	break;
		case 6:	     send_threelines(0x14);	break;
		case 7:	 	 send_threelines(0x15);	break;
		case 8:	 	 send_threelines(0x16);	break;
		case 9:	 	 send_threelines(0x17);	break;
		case 10:	 send_threelines(0x18);	break;
		case 11:	 send_threelines(0x19);	break;
		case 12:	 send_threelines(0x1A);	break;
		case 13:	 send_threelines(0x1B);	break;
		case 14:	 send_threelines(0x1C);	break;
		case 15:	 send_threelines(0x1D);	break;
		case 16:	 send_threelines(0x1E);	break;
		case 17:	 send_threelines(0x1F);	break;
		case 18:	 send_threelines(0x20);	break;
		case 19:	 send_threelines(0x21);	break;
		case 20:	 send_threelines(0x22);	break;
		case 21:	 send_threelines(0x23);	break;
		case 22:	 send_threelines(0x24);	break;
		case 23:	 send_threelines(0x25);	break;
		default:	 break;
	}
}


//--------------------定时器中断t0初始化函数-------------------//
void t0_init()
{
	TMOD |= 0x01;		//定时器0工作模式为 MODEL 1,timer 
	TH0 = 0xFC;
	TL0 = 0x18;
	ET0 = 1;			//定时器0中断使能	
	IP = 0x02;			//设定定时器0中断优先级最高 
	TR0 = 1;			//定时器启动 
}

//----------------------内部定时器t0中断函数----------------------//
void t0_int() interrupt 1
{
	TH0 = 0xFC;
	TL0 = 0x18;
	
	EA = 0;						//暂时关闭总中断 	
	t0_count++;
	if(t0_count==1000){
		t0_count = 0;
		miao_inc();				//秒+1
		display_time();			//显示当前时间
		if((miao==0)&&(fen==0))	//当整点时报时
			alarm_int();
	}
	EA = 1;						//开总中断
}		

//-------------------------按键检测--------------------------//
void key()
{
	if(!vol_set_key){
		delay50us(4);		//消抖
		if(!vol_set_key){
			vol = vol+1;
			if(vol>0xE7)	
				vol = 0xE2;	 //音量值最大为E7H	
			while(!vol_set_key);
		    vol_adj(vol);	//调用音量调节子函数
		}
	}
	if(!alarm_key){
		delay50us(4);		//消抖
		if(!alarm_key){
			while(!alarm_key);
		    alarm_now(shi,fen);		//当前时间语音播报
		}
	}	 
	if(!time_selc_key){
		delay50us(4);
		if(!time_selc_key){
			flag = (flag+1)%4;
			display(14,1,flag+1);
			while(!time_selc_key);			
		}
	}
	if(!time_adj_key){
		delay50us(4);
		if(!time_adj_key){
			switch(flag){
				case 0:	gao = (fen/10)%10;
						di = (di+1)%10;  
						fen = gao*10 + di;
						break;
				case 1:	di = fen%10;
						gao = (gao+1)%6;
						fen = gao*10 + di;
						break;
				case 2:	gao = (shi/10)%10;
						di = (di+1)%10; 
						shi = gao*10 + di;
						if(shi>23) 
							shi = 20;
						break;
				case 3:	di = shi%10;
						gao = (gao+1)%3;
						shi = gao*10 + di;
						if(shi>23) 
							shi = di;
						break;
				default: break;
			}
		}
		while(!time_adj_key);
	}
	
}	 

//-------------------------主函数------------------------------//
void main()
{	
	EA = 1;				   //开总中断
	vol_adj(0xE3);		   //预设音量
	lcd_init();
	lcd_pos(2,0);
	prints("Present Time");
	lcd_pos(6,1);
	prints(":");
	lcd_pos(9,1);
	prints(":"); 	
	t0_init();
	display_time();			//显示当前时间
	vol_adj(0xE5);			//设定初始音量
							
	while(1){
		key(); 	
	}
}
