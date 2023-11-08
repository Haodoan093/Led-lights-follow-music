#include <arduinoFFT.h>
#include <MD_MAX72xx.h>
#include <SPI.h>

#define SAMPLES 64            // Phải là lũy thừa của 2
#define LOAI_THIET_BI MD_MAX72XX::FC16_HW   // Chọn loại hiển thị để MD_MAX72xx library hiểu đúng cách
#define TONG_SO_THIET_BI 4   // Tổng số module hiển thị
#define CLK_PIN 13  // Chân Clock để giao tiếp với hiển thị
#define DATA_PIN 11  // Chân Data để giao tiếp với hiển thị
#define CS_PIN 10  // Chân điều khiển để giao tiếp với hiển thị
#define XRES 32      // Tổng số cột trên hiển thị, phải nhỏ hơn hoặc bằng SAMPLES/2
#define YRES 8       // Tổng số hàng trên hiển thị

double vReal[SAMPLES];
double vImag[SAMPLES];
char data_avgs[XRES];

int yvalue;
int displaycolumn, displayvalue;
int peaks[XRES];
const int buttonPin = 5;    // Số chân nút nhấn
int state = HIGH;             // Giá trị hiện tại từ chân đầu vào
int previousState = LOW;   // Giá trị trước đây từ chân đầu vào
int displaymode = 1;
unsigned long lastDebounceTime = 0;  // Thời gian cuối cùng chân ra ngoài được chuyển đổi
unsigned long debounceDelay = 50;    // Thời gian chờ ổn định; tăng nếu chân ra ngoài bị nhấp nháy
int value=0;
MD_MAX72XX mx = MD_MAX72XX(LOAI_THIET_BI, CS_PIN, TONG_SO_THIET_BI);   // Đối tượng hiển thị
arduinoFFT FFT = arduinoFFT();                                    // Đối tượng FFT


int MY_ARRAY[] = {0, 128, 64, 32, 16, 8, 4, 2, 1};; // mẫu mặc định = mẫu tiêu chuẩn
int MY_MODE_1[] = {0, 128, 192, 224, 240, 248, 252, 254, 255}; // Mẫu tiêu chuẩn
int MY_MODE_2[] = {0, 128, 64, 32, 16, 8, 4, 2, 1}; // Chỉ mẫu đỉnh
int MY_MODE_3[] = {0, 128, 192, 160, 144, 136, 132, 130, 129}; // Chỉ mẫu đỉnh + điểm dưới cùng
int MY_MODE_4[] = {0, 128, 192, 160, 208, 232, 244, 250, 253}; // Một khoảng trống ở phía trên, từ đèn thứ 3 trở đi
int MY_MODE_5[] = {0, 1, 3, 7, 15, 31, 63, 127, 255}; // Mẫu tiêu chuẩn, đối xứng theo chiều dọc
int MY_MODE_6[] = {0, 1, 2, 4, 8, 16, 32, 64, 128}; // Chỉ mẫu đỉnh ngược
int MY_MODE_7[] = {0, 32, 64, 96, 128, 160, 192, 224, 255}; // Một chế độ sáng dần



void setup() {
    ADCSRA = B11100101;      // Đặt ADC sang chế độ chạy tự do và đặt tiền chia tới 32 (0xe5)
    ADMUX = B01000000; // Sử dụng chân A0 và tham chiếu điện áp nội (VCC)
   
    //   đọc từ chân A0 có thể không hoạt động đúng cách hoặc gây cố định chương trình.
    pinMode(buttonPin, INPUT);
    mx.begin();  
     Serial.begin(9600);         // Khởi tạo hiển thị
    delay(50);            // Chờ để ổn định điện áp tham chiếu
}

void loop() {
  
    // ++ Lấy mẫu
    for (int i = 0; i < SAMPLES; i++) {
        while (!(ADCSRA & 0x1)); // Chờ ADC hoàn thành chuyển đổi hiện tại, tức là bit ADIF được đặt
        ADCSRA = B11110101; // Xóa bit ADIF để ADC có thể thực hiện thao tác tiếp theo (0xf5)
         value = ADC - 512; // Đọc từ ADC và trừ đi DC offset gây ra giá trị
        vReal[i] = value / 8; // Sao chép vào các khoảng sau khi nén
        vImag[i] = 0;
       
    }
 
    // -- Lấy mẫu

    // ++ FFT
    FFT.Windowing(vReal, SAMPLES, FFT_WIN_TYP_HAMMING, FFT_FORWARD);
    FFT.Compute(vReal, vImag, SAMPLES, FFT_FORWARD);
    FFT.ComplexToMagnitude(vReal, vImag, SAMPLES);
    // -- FFT

    // ++ Sắp xếp lại kết quả FFT để phù hợp với số cột trên hiển thị (XRES)
    int buoc = (SAMPLES / 2) / XRES;
    int c = 0;
    for (int i = 0; i < (SAMPLES / 2); i += buoc) {
        data_avgs[c] = 0;
         for (int k = 0; k < buoc; k++) {
             data_avgs[c] = data_avgs[c] + vReal[i + k];
         }
         data_avgs[c] = data_avgs[c] / buoc;
        c++;
    }
    // -- Sắp xếp lại kết quả FFT để phù hợp với số cột trên hiển thị (XRES)

    // ++ Gửi tới hiển thị theo giá trị đo được
   
     for (int i = 0; i < XRES; i++) {
        data_avgs[i] = constrain(data_avgs[i], 0, 80); // Đặt giá trị tối đa và tối thiểu cho các khoảng
        data_avgs[i] = map(data_avgs[i], 0, 80, 0, YRES); // Ánh xạ lại giá trị trung bình thành YRES
        yvalue = data_avgs[i];

        peaks[i] = peaks[i] - 1; // Giảm đi một đèn
        if (yvalue > peaks[i])
            peaks[i] = yvalue;
        yvalue = peaks[i];
        displayvalue = MY_ARRAY[yvalue];
        displaycolumn = 31 - i;
        
        mx.setColumn(displaycolumn, displayvalue); // Cho từ trái sang phải
    
   }
    // -- Gửi tới hiển thị theo giá trị đo được

    displayModeChange(); // Kiểm tra nút đã được nhấn để thay đổi chế độ hiển thị
}

void displayModeChange() {
    int reading = digitalRead(buttonPin);
    if (reading == HIGH && previousState == LOW && millis() - lastDebounceTime > debounceDelay) {
        displaymode++;
        if (displaymode > 7) {
            displaymode = 1; // Quay lại chế độ đầu tiên nếu đã đi qua tất cả các chế độ
        }
        
        switch (displaymode) {
            case 1:
                for (int i = 0; i <= 8; i++) {
                    MY_ARRAY[i] = MY_MODE_1[i];
                }
                Serial.println("Mode 1");
                break;
            case 2:
                for (int i = 0; i <= 8; i++) {
                    MY_ARRAY[i] = MY_MODE_2[i];
                }
                Serial.println("Mode 2");
                break;
            case 3:
                for (int i = 0; i <= 8; i++) {
                    MY_ARRAY[i] = MY_MODE_3[i];
                }
                Serial.println("Mode 3");
                break;
            case 4:
                for (int i = 0; i <= 8; i++) {
                    MY_ARRAY[i] = MY_MODE_4[i];
                }
                Serial.println("Mode 4");
                break;
            case 5:
                for (int i = 0; i <= 8; i++) {
                    MY_ARRAY[i] = MY_MODE_5[i];
                }
                Serial.println("Mode 5");
                break;
            case 6:
                for (int i = 0; i <= 8; i++) {
                    MY_ARRAY[i] = MY_MODE_6[i];
                }
                Serial.println("Mode 6");
                break;
            case 7:
                for (int i = 0; i <= 8; i++) {
                    MY_ARRAY[i] = MY_MODE_7[i];
                }
                Serial.println("Mode 7");
                break;
     
        }
        
        lastDebounceTime = millis();
    }
    previousState = reading;
}

