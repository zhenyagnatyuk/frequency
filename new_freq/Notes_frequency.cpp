#include "stdafx.h"


#if defined(_M_X64) || defined(__amd64__)
#pragma comment(lib, "portaudio_x64.lib")
#else
#pragma comment(lib, "portaudio_x86.lib")
#endif

#define DELTA_NOTE 49

#define SAMPLE_RATE (48000)
#define FRAMES_PER_BUFFER (8192)
#define NUM_CHANNELS (2)

/* Select sample format. */
#define PA_SAMPLE_TYPE paFloat32  
#define SAMPLE_SIZE (4)
#define CLEAR(a) memset( (a), 0, FRAMES_PER_BUFFER * NUM_CHANNELS * SAMPLE_SIZE ) // обнуление массива
#define PRINTF_S_FORMAT "%.8f"


const double PI = 3.141592653589793238460;

typedef std::complex<double> Complex;
typedef std::valarray<Complex> CArray;


struct {
	int num;
	char letter[5];
	float freq;
	float freq_down;
	float freq_up;
} note[108];

void clrscr();
void threadout(char &c);
void FreqOfNotes();
void fft(CArray& x);
void VerPar(float x1, float y1, float x2, float y2, float x3, float y3);
float X0, Y0;

int main() {
	FreqOfNotes();// заполняем структуру нот

	PaStreamParameters inputParameters;
	PaStream *stream = NULL;
	//PaError err;
	
	float *sampleBlock, len_vector;
	int i, j, note_real;
	float max_ampl;
	int max_i;
	int numBytes;
	float Im[FRAMES_PER_BUFFER], freq_real, freq_real_data;
	//fflush(stdout);


	numBytes = FRAMES_PER_BUFFER * NUM_CHANNELS * SAMPLE_SIZE;
	sampleBlock = (float *)malloc(numBytes);

	CLEAR(sampleBlock);

	Pa_Initialize();
	inputParameters.device = Pa_GetDefaultInputDevice();
	inputParameters.channelCount = NUM_CHANNELS;
	inputParameters.sampleFormat = PA_SAMPLE_TYPE;
	inputParameters.suggestedLatency = Pa_GetDeviceInfo(inputParameters.device)->defaultHighInputLatency;
	inputParameters.hostApiSpecificStreamInfo = NULL;

	Pa_OpenStream(
		&stream,
		&inputParameters,
		0,
		SAMPLE_RATE,
		FRAMES_PER_BUFFER,
		paClipOff,
		NULL,
		NULL
	);

	
	Pa_StartStream(stream);
	
	clrscr();

	printf("**********************************************************************************************************\n");
	printf("                                     READING STREAM(Enter to stop).\n");
	printf("**********************************************************************************************************\n");
	fflush(stdout);
	
	char c;
	
	std::thread thr(threadout, std::ref(c));
	
	while (1) {

		if (c == 13) {
			if (thr.joinable())
				thr.detach();
			break;
		}

		Pa_ReadStream(stream, sampleBlock, FRAMES_PER_BUFFER);

		printf("\r");

		Complex test[FRAMES_PER_BUFFER];
		
		for (j = 0; j < FRAMES_PER_BUFFER; j++) {
			test[j] = sampleBlock[j];
		}
		
		CArray data(test, FRAMES_PER_BUFFER);
		
		// определяем максимальную амплитуду
		double max_sample = 0;
		for (j = 0; j < FRAMES_PER_BUFFER; ++j) {
			if (data[j].real() > max_sample)
				max_sample = data[j].real();
		}
		printf("Amplitude1=%3.0f  \t", max_sample * 100);
		
		// если аплтитуда мала, ничего не анализируем!
		if (max_sample>0.2) {
			// отправляем массив на БПФ
			fft(data);
			// определяем одну из двух частот с максимальным вектором
			max_i = 0;
			max_ampl = 0;

			for (i = 0; i<FRAMES_PER_BUFFER / 100; i++) {
				len_vector = (float)sqrt(data[i].real() * data[i].real() + data[i].imag() * data[i].imag());
				if (len_vector>max_ampl) {
					max_ampl = len_vector;
					max_i = i;
				}
			}
			printf("Freq= %f  \t", (float)max_i*SAMPLE_RATE / FRAMES_PER_BUFFER); // показывает частоту

			double len_vector_1 = sqrt(data[max_i - 1].real() * data[max_i - 1].real() + data[max_i - 1].imag() * data[max_i - 1].imag());
			double len_vector_2 = sqrt(data[max_i + 1].real() * data[max_i + 1].real() + data[max_i + 1].imag() * data[max_i + 1].imag());
			
			VerPar((float)max_i - 1, (float)len_vector_1, (float)max_i, (float)len_vector, (float)max_i + 1, (float)len_vector_2);


			freq_real = X0 * SAMPLE_RATE / FRAMES_PER_BUFFER;
			note_real = 0;

			for (i = 28; i <= 51; i++) {
				if (freq_real > note[i].freq_down && freq_real < note[i].freq_up) {
					note_real = i;
					break;
				}
			}
			if (note_real>28) {
				printf("FreqReal= %5.2f  \t", freq_real); // уточненная частота
				printf("Note= %s\t", note[note_real].letter);
				// печатаем отклонение в центах
				printf("delta= %f   \n", 1200 * log2(freq_real / note[note_real].freq));
				printf("\n");
			}

			fflush(stdout);
		}
	}
	CLEAR(sampleBlock);
	free(sampleBlock);

	Pa_StopStream(stream);
	Pa_Terminate();

	return 0;
}

void threadout(char &c) {
	c = _getch();
}

void fft(CArray& x) {
	const size_t N = x.size();
	if (N <= 1) return;

	// divide
	CArray even = x[std::slice(0, N / 2, 2)];
	CArray  odd = x[std::slice(1, N / 2, 2)];

	// conquer
	fft(even);
	fft(odd);

	// combine
	for (size_t k = 0; k < N / 2; ++k) {
		Complex t = std::polar(1.0, -2 * PI * k / N) * odd[k];
		x[k] = even[k] + t;
		x[k + N / 2] = even[k] - t;
	}
}


void VerPar(float x1, float y1, float x2, float y2, float x3, float y3) {
	// определяем координаты вершины параболы, выводим в глобал переменные
	float A, B, C;
	
	A = (y3 - ((x3*(y2 - y1) + x2 * y1 - x1 * y2) / (x2 - x1))) / (x3*(x3 - x1 - x2) + x1 * x2);
	B = (y2 - y1) / (x2 - x1) - A * (x1 + x2);
	C = ((x2*y1 - x1 * y2) / (x2 - x1)) + A * x1*x2;
	
	X0 = -(B / (2 * A));
	Y0 = -((B*B - 4 * A*C) / (4 * A));
}

void clrscr() {
	system("@cls||clear");
}
void FreqOfNotes() {
	int i;
	const char *letters[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
	const float cent = 1.0 / 1200;//1 cent https://ru.wikipedia.org/wiki/%D0%A6%D0%B5%D0%BD%D1%82_(%D0%BC%D1%83%D0%B7%D1%8B%D0%BA%D0%B0)
	
	for (i = 20; i <= 128; i++) {
		int j = i - 20;
		note[j].num = j;

		strcat_s(note[j].letter, 5, letters[i % 12]);

		if (i >= 24 && i <= 35) strcat_s(note[j].letter, 5, "1");
		else if (i >= 36 && i <= 47) strcat_s(note[j].letter, 5, "2");
		else if (i >= 48 && i <= 59) strcat_s(note[j].letter, 5, "3");
		else if (i >= 60 && i <= 71) strcat_s(note[j].letter, 5, "4");
		else if (i >= 72 && i <= 83) strcat_s(note[j].letter, 5, "5");
		else if (i >= 84 && i <= 95) strcat_s(note[j].letter, 5, "6");
		else if (i >= 96 && i <= 107) strcat_s(note[j].letter, 5, "7");

		note[j].freq = 440 * powf(2, (i - 69) / 12.0);
		note[j].freq_up = note[j].freq* powf(powf(2, cent), DELTA_NOTE); // смещение от ноты в центах
		note[j].freq_down = note[j].freq* powf(powf(2, cent), -DELTA_NOTE); // смещение от ноты в центах
	}
}