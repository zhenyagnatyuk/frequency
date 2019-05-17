#include "stdafx.h"


#if defined(_M_X64) || defined(__amd64__)
	#pragma comment(lib, "portaudio_x64.lib")
#else
	#pragma comment(lib, "portaudio_x86.lib")
#endif

#define DELTA_NOTE 49

#define SAMPLE_RATE (48000)
//       N    = 4, 8, 16, 32, 64, 128, 256, 512, 1024, 2048, 4096, 8192, 16384;
//       LogN = 2, 3,  4,  5,  6,   7,   8,   9,   10,   11,   12,   13,    14;
#define FRAMES_PER_BUFFER (8192)
#define POW_2 (13)
#define NUM_CHANNELS (2)

/* Select sample format. */
#define PA_SAMPLE_TYPE paFloat32  
#define SAMPLE_SIZE (4)
#define SAMPLE_SILENCE (0.0f)
#define CLEAR(a) memset( (a), 0, FRAMES_PER_BUFFER * NUM_CHANNELS * SAMPLE_SIZE ) // обнуление массива
#define PRINTF_S_FORMAT "%.8f"

#define  NUMBER_IS_2_POW_K(x)   ((!((x)&((x)-1)))&&((x)>1))  // x is pow(2, k), k=1,2, ...
#define  FT_DIRECT        -1    // Direct transform.
#define  FT_INVERSE        1    // Inverse transform.

struct{
	int num;
	char letter[5];
	float freq;
	float freq_down;
	float freq_up;
} note[108];

void clrscr();
void threadout(char &c);
void FreqOfNotes();
void FFT(float *Rdat, float *Idat, int N, int LogN, int Ft_Flag);
void VerPar(float x1, float y1, float x2, float y2, float x3, float y3);
float X0, Y0;

int main(){
	FreqOfNotes();// заполняем структуру нот
	PaStreamParameters inputParameters;
	PaStream *stream = NULL;
	//PaError err;
	float *sampleBlock, len_vector;
	int i, j, note_real;
	float max_ampl;
	int max_i;
	int numBytes;
	float Im[FRAMES_PER_BUFFER], freq_real;
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
		NULL);
	/* потом прописать через try, ибо Upon success Pa_OpenStream() returns paNoError and places a pointer 
	to a valid PaStream in the stream argument. The stream is inactive (stopped). 
	If a call to Pa_OpenStream() fails, a non-zero error code is returned (see PaError for possible error codes) and the value of stream is invalid.*/
	Pa_StartStream(stream);
	clrscr();
	//printf("Device count = %i\n", Pa_GetDeviceCount());
	//printf("Note= %f\t", note[80].freq);
	printf("**************************************************\n");
	printf("               READING STREAM(Enter to stop).\n");
	printf("**************************************************\n");
	fflush(stdout);
	// прописано "Enter to stop"
	char c;
	std::thread thr(threadout, std::ref(c));
	while(1){
		
		if (c == 13) break;
		if(thr.joinable())
			thr.detach();

		float max_sample = 0;
		Pa_ReadStream(stream, sampleBlock, FRAMES_PER_BUFFER);

		printf("\r");
		// определяем максимальную амплитуду
		for (j = 0; j < FRAMES_PER_BUFFER; ++j){
			if (sampleBlock[j] > max_sample) 
				max_sample = sampleBlock[j];
		}
		printf("Amplitude=%3.0f  \t", max_sample * 100);

		// ***************************************
		// если аплтитуда мала, ничего не анализируем!

		if (max_sample>0.2){
			// отправляем массив на БПФ, обнулив мнимую часть
			for (i = 0; i<FRAMES_PER_BUFFER; i++) Im[i] = 0.0;
			FFT(sampleBlock, Im, FRAMES_PER_BUFFER, POW_2, -1);

			// определяем одну из двух частот с максимальным вектором
			max_i = 0;
			max_ampl = 0;
			for (i = 0; i<FRAMES_PER_BUFFER / 100; i++){
				len_vector = sqrt(sampleBlock[i] * sampleBlock[i] + Im[i] * Im[i]);
				if (len_vector>max_ampl){
					max_ampl = len_vector;
					max_i = i;
				}
			}
			printf("Freq= %f  \t",(float)max_i*SAMPLE_RATE/FRAMES_PER_BUFFER); // показывает частоту
			float len_vector_1 = sqrt(sampleBlock[max_i - 1] * sampleBlock[max_i - 1] + Im[max_i - 1] * Im[max_i - 1]);
			float len_vector_2 = sqrt(sampleBlock[max_i + 1] * sampleBlock[max_i + 1] + Im[max_i + 1] * Im[max_i + 1]);
			VerPar((float)max_i - 1, len_vector_1, (float)max_i, len_vector, (float)max_i + 1, len_vector_2);

			freq_real = X0 * SAMPLE_RATE / FRAMES_PER_BUFFER;
			//Beep(freq_real, max_sample*100);
			// определяем номер ноты и пишем буквенное обозначение
			note_real = 0;
			for (i = 20; i <= 83; i++){
				if (freq_real > note[i].freq_down && freq_real < note[i].freq_up) {
					note_real = i;
					break;
				}
			}
			if (note_real>20){
				printf("Note num = %d\t", note_real);
				printf("FreqReal= %5.2f  \t",freq_real); // уточненная частота
				printf("Note= %s\t", note[note_real].letter);
				// печатаем отклонение в центах
				// 1 цент = 
				printf("delta= %f   \n", 1200 * log2(freq_real / note[note_real].freq));

			}
			fflush(stdout);
			/**/
			
		}

		else printf("\t\t\t\t\t\t");
	}
	CLEAR(sampleBlock);
	free(sampleBlock);

	Pa_StopStream(stream);
	Pa_Terminate();
	return 0;
}
void threadout(char &c){
	c = _getch();
	
}
/*void fft(CArray& x)
{
	const size_t N = x.size();
	if (N <= 1) return;

	// divide
	CArray even = x[std::slice(0, N / 2, 2)];
	CArray  odd = x[std::slice(1, N / 2, 2)];

	// conquer
	fft(even);
	fft(odd);

	// combine
	for (size_t k = 0; k < N / 2; ++k)
	{
		Complex t = std::polar(1.0, -2 * PI * k / N) * odd[k];
		x[k] = even[k] + t;
		x[k + N / 2] = even[k] - t;
	}
}*/

void  FFT(float *Rdat, float *Idat, int N, int LogN, int Ft_Flag){
	// parameters error check:
	if ((Rdat == NULL) || (Idat == NULL))
		return;
	if ((N > 16384) || (N < 1))
		return;
	if (!NUMBER_IS_2_POW_K(N))
		return;
	if ((LogN < 2) || (LogN > 14))
		return;
	if ((Ft_Flag != FT_DIRECT) && (Ft_Flag != FT_INVERSE))
		return;

	register int  i, j, n, k, io, ie, in, nn;
	float         ru, iu, rtp, itp, rtq, itq, rw, iw, sr;

	static const float Rcoef[14] ={
		-1.0000000000000000F,  0.0000000000000000F,  0.7071067811865475F,
		0.9238795325112867F,  0.9807852804032304F,  0.9951847266721969F,
		0.9987954562051724F,  0.9996988186962042F,  0.9999247018391445F,
		0.9999811752826011F,  0.9999952938095761F,  0.9999988234517018F,
		0.9999997058628822F,  0.9999999264657178F
	};
	static const float Icoef[14] ={
		0.0000000000000000F, -1.0000000000000000F, -0.7071067811865474F,
		-0.3826834323650897F, -0.1950903220161282F, -0.0980171403295606F,
		-0.0490676743274180F, -0.0245412285229122F, -0.0122715382857199F,
		-0.0061358846491544F, -0.0030679567629659F, -0.0015339801862847F,
		-0.0007669903187427F, -0.0003834951875714F
	};

	nn = N >> 1;
	ie = N;
	for (n = 1; n <= LogN; n++){
		rw = Rcoef[LogN - n];
		iw = Icoef[LogN - n];
		if (Ft_Flag == FT_INVERSE) iw = -iw;
		in = ie >> 1;
		ru = 1.0F;
		iu = 0.0F;
		for (j = 0; j<in; j++){
			for (i = j; i<N; i += ie){
				io = i + in;
				rtp = Rdat[i] + Rdat[io];
				itp = Idat[i] + Idat[io];
				rtq = Rdat[i] - Rdat[io];
				itq = Idat[i] - Idat[io];
				Rdat[io] = rtq * ru - itq * iu;
				Idat[io] = itq * ru + rtq * iu;
				Rdat[i] = rtp;
				Idat[i] = itp;
			}

			sr = ru;
			ru = ru * rw - iu * iw;
			iu = iu * rw + sr * iw;
		}

		ie >>= 1;
	}

	for (j = i = 1; i<N; i++){
		if (i < j)
		{
			io = i - 1;
			in = j - 1;
			rtp = Rdat[in];
			itp = Idat[in];
			Rdat[in] = Rdat[io];
			Idat[in] = Idat[io];
			Rdat[io] = rtp;
			Idat[io] = itp;
		}

		k = nn;

		while (k < j){
			j = j - k;
			k >>= 1;
		}

		j = j + k;
	}

	if (Ft_Flag == FT_DIRECT) return;

	rw = 1.0F / N;

	for (i = 0; i<N; i++){
		Rdat[i] *= rw;
		Idat[i] *= rw;
	}

	return;
}

void VerPar(float x1, float y1, float x2, float y2, float x3, float y3){
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
void FreqOfNotes(){
	int i;
	const char *letters[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
	const float cent = 1.0 / 1200;//1 cent https://ru.wikipedia.org/wiki/%D0%A6%D0%B5%D0%BD%D1%82_(%D0%BC%D1%83%D0%B7%D1%8B%D0%BA%D0%B0)
	for (i = 20; i <= 128; i++){
		int j = i - 20;
		note[j].num = j;

		strcat_s(note[j].letter, 5, letters[i % 12]);

		if (i >= 24 && i <= 35) strcat_s(note[j].letter, 5, "1");
		else if (i >= 36 && i <= 47) strcat_s(note[j].letter, 5, "2");
		else if (i >= 48 && i <= 59) strcat_s(note[j].letter, 5, "3");
		else if (i >= 60 && i <= 71) strcat_s(note[j].letter, 5, "4");
		else if (i >= 72 && i <= 83) strcat_s(note[j].letter, 5, "5");
		else if (i >= 84 && i <= 95) strcat_s(note[j].letter, 5, "6");
		else if (i >= 96 && i <= 107) strcat_s(note[j].letter, 5, "6");

		note[j].freq = 440 * powf(2, (i - 69) / 12.0);
		note[j].freq_up = note[j].freq* powf(powf(2, cent), DELTA_NOTE); // смещение от ноты в центах
		note[j].freq_down = note[j].freq* powf(powf(2, cent), -DELTA_NOTE); // смещение от ноты в центах
	}
}

