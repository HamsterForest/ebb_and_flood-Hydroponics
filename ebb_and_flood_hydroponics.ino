#include <LiquidCrystal_I2C.h>//lcd사용 
#include <avr/wdt.h>//워치독 타이머 일정 시간 동안 리셋되지 않으면 아두이노를 리셋

LiquidCrystal_I2C lcd(0x27, 20, 4);
extern volatile unsigned long timer0_millis;

#define PUMPTIMER 120000 //110초 이후 센서가 작동하지 않아도 펌프를 자동으로 종료
#define PUMPTIMING 1800000 //30분마다 펌프 작동=> 60*30*1000
#define LCDTIMING 30000//30초마다 LCD업데이트 -LCD오류 방지
 
void lcd_control(double currentHour, int watering_count,bool led_check){

  lcd.setCursor(0, 0);   
  lcd.print("Aqua_culture by kkh");//19
  lcd.setCursor(0, 1);
  lcd.print("Time 0-24 : ");//14
  lcd.setCursor(13,1);
  lcd.print(currentHour);
  lcd.setCursor(0, 2);
  lcd.print("Watered: ");//9
  lcd.setCursor(9, 2);
  lcd.print(watering_count);
  lcd.setCursor(0, 3);
  lcd.print("Led state: ");//11
  lcd.setCursor(11, 3);
  if(led_check==false){
    lcd.print("light OUT");
  }
  else{
    lcd.print("light ON");
  }
}

bool watering(unsigned int long start_time){//물주기, 수위조절 current_time을 이용해 센서가 작동하지 않을 때를 대비함-일정시간후 펌프종료,반환값은 센서 작동여부 true=>작동
  unsigned int long limit_time=PUMPTIMER;
  unsigned int long current_time=0;
  lcd.clear();
  lcd.setCursor(0,1);
  lcd.print("Watering!!!");

  while(1){
    wdt_reset();
    digitalWrite(9, HIGH);//펌프작동
    if(digitalRead(12)==1){//수위조절센서
      digitalWrite(9,LOW);
      lcd.clear();
      return true;
    }

    current_time=millis();//현재시간
    if((current_time-start_time)>=limit_time){//펌프작동후 경과된 시간을 limit_time과 비교
      digitalWrite(9,LOW);
      lcd.clear();
      lcd.setCursor(0,1);
      lcd.print("Sensor fail");
      wdt_disable();//delay함수를 사용하기 위해 워치독을 비활성화
      delay(10000);
      lcd.clear();
      wdt_enable(WDTO_8S);//워치독 다시 사용
      return false;
    }
  }
}

void setup()
{
  lcd.begin(); //LCD 사용 시작 20x4 lcd임
  delay(3000);
  pinMode(8, OUTPUT);//led용 릴레이
  pinMode(9, OUTPUT);//펌프용 릴레이
  pinMode(12, INPUT);//수위센서
  wdt_enable(WDTO_8S);
}

bool watering_trigger(unsigned int long currentTime, int time_check_w){//30분단위의 타이머-30분 마다 트리거 하며 트리거의 여부를 반환
  int current_halfHour=currentTime/PUMPTIMING;//30분단위의 시간 측정-30분마다 값을 업데이트한다. 0->1->2->3 ......
  if(current_halfHour!=time_check_w){//current_halfHour가 변화하는 경우 true반환
    return true;
  }
  else{
    return false;
  }
  /*time_check_w는 기계가 작동을 시작하면 0에서 시작한다. current_halfhour는 항시 시간을 30분 단위로 업데이트 한다. 
    만약 current_halfhour가 새로운 값으로 업데이트되면 time_check_w와 값이 달라지게되고 if문을 통해 이를 알아낼 수 있다.
    변화를 감지한 이후에 true를 반환하여 watering함수가 작동할 수 있게 한다. 또한 time_check_w는 성공적으로 30분단위 시간 변화를
    감지후 기존의 current_halfHour와 동일한 값으로 바꾼다.(함수외부에서)*/
}

bool lcd_trigger(unsigned int long currentTime, int time_check_l){
  int current_halfMin=currentTime/LCDTIMING;
  if(current_halfMin!=time_check_l){
    return true;
  }
  else{
    return false;
  }
}

void loop()
{
  int watering_count=0;//24시간 주기안에서 물을 준 횟수를 카운트
  int time_check_w=0;//30분마다 물을 주는것을 보장하기위한 변수
  int time_check_l=0;//30초마다 lcd를 업데이트 하는것을 보장하기 위한 변수=>lcd오류 방지
  bool led_check=true;//led의 점등,소등여부
  unsigned int long currentTime=millis();
  //초기 작동
  digitalWrite(8,HIGH);//led작동
  if(watering(currentTime)){
    watering_count++;
  }
  

  //메인 루프
  while(1){
    wdt_reset();
    currentTime=millis();//작동후 경과한 시간을 밀리초단위로 반환
    
    double currentHour=floor(double(currentTime)/3600000.0*100)/100; //밀리초 단위로 반환된 시간을 시간단위로 소수점으로 나타내며 lcd에 표시하기 위함
  
    lcd_control(currentHour,watering_count,led_check);//lcd업데이트 
    bool trigger_l=lcd_trigger(currentTime,time_check_l);
    
    if(trigger_l){
      time_check_l=currentTime/LCDTIMING;
      lcd.begin();
      //Serial.println("lcd cleared");
    }
    
    bool trigger_w=watering_trigger(currentTime,time_check_w);

    if(trigger_w){
      time_check_w=currentTime/PUMPTIMING;
      if(watering(currentTime)){
        watering_count++;
      }
    }

    if(currentTime>=57600000){//16시간이후 led 소등
      digitalWrite(8,LOW);
      led_check=false;//정상적으로 led 소등
   }

    if(currentTime>=86400000){//24시간이후 타이머 초기화
      break;
    }
  }//대while문
  
}
