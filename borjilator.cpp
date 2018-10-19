#include "borjilator.hpp"

#if PRINT_MODE == MACHINE
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#endif
int RECURSION_LEVEL = 13;
int MATADES_MULT = 300;
int MORTES_MULT = 400;

static short g_terminate = 0;
static bool g_clock_ticking = true;
static clock_t g_start_time = 0;
static int jugada = 1;

#ifdef DEBUG
static uint64_t used_memoizes = 0;
static std::mutex mtx2;
#endif

static std::shared_mutex mtx;

bool operator==(IDj &a, IDj &b){
	return (a.t.l[0] == b.t.l[0] && a.t.l[1] == b.t.l[1] && a.score[0] == b.score[0] && a.score[1] == b.score[1]);
}
bool operator==(const IDj &a, const IDj &b){
	return (a.t.l[0] == b.t.l[0] && a.t.l[1] == b.t.l[1] && a.score[0] == b.score[0] && a.score[1] == b.score[1]);
}
bool operator!=(IDj &a, IDj &b){
	return !(a.t.l[0] == b.t.l[0] && a.t.l[1] == b.t.l[1] && a.score[0] == b.score[0] && a.score[1] == b.score[1]);
}
std::istream& operator>>(std::istream &in, IDj &a) {
	in.read((char*)a.t.c, 6*sizeof(uint8_t));
	in.read((char*)&a.t.c[8], 6*sizeof(uint8_t));
	in.read((char*)&a.score[0], sizeof(uint8_t));
	in.read((char*)&a.score[1], sizeof(uint8_t));
	return in;
}
std::ostream& operator<<(std::ostream &out, IDj &a) {
	out.write((char*)a.t.c, 6*sizeof(uint8_t));
	out.write((char*)&a.t.c[8], 6*sizeof(uint8_t));
	out.write((char*)a.score, 2*sizeof(uint8_t));
	return out;
}
/*std::ostream& operator<<(std::ostream &out, const IDj &a) {
	//out.write((char*)a.t.c, 6*sizeof(uint8_t));
	//out.write((char*)&a.t.c[8], 6*sizeof(uint8_t));
	out << std::endl;
	out << "C=" << (int)a.t.c[0] << " " << (int)a.t.c[1] << " " << (int)a.t.c[2] << " " <<  (int)a.t.c[3] << " " <<  (int)a.t.c[4] << " " << (int)a.t.c[5] << " " << std::endl;
	out << "C=" << (int)a.t.c[8] << " " << (int)a.t.c[9] << " " << (int)a.t.c[10] << " " <<  (int)a.t.c[11] << " " <<  (int)a.t.c[12] << " " << (int)a.t.c[13] << " " << std::endl;
	return out;
}*/
std::istream& operator>>(std::istream &in, std::tuple<int,short,unsigned short> &vmr) {
	char v[4];
	char m[2];
	char r[2];
	in.read(v, vsize);
	in.read(m, msize);
	in.read(r, rsize);
	for (int i = 0; i < vsize; i++)
		vmr.v = (vmr.v<<8) + (unsigned char)v[i];
	for (int i = 0; i < msize; i++)
		vmr.m = (vmr.m<<8) + (unsigned char)m[i];
	for (int i = 0; i < rsize; i++)
		vmr.r = (vmr.r<<8) + (unsigned char)r[i];

	return in;
}


std::ostream& operator<<(std::ostream &out, memItem &vmr) {
	size_t vsize = sizeof(vmr.v);
	size_t msize = sizeof(vmr.m);
	size_t rsize = sizeof(vmr.r);
	char v[vsize];
	char m[msize];
	char r[rsize];
	for (int i = 0; i < vsize; i++)
		v[vsize - 1 - i] = (vmr.v >> (i * 8));
	for (int i = 0; i < msize; i++)
		m[msize - 1 - i] = (vmr.m >> (i * 8));
	for (int i = 0; i < rsize; i++)
		r[rsize - 1 - i] = (vmr.r >> (i * 8));
	out.write(v, vsize);
	out.write(m, msize);
	out.write(r, rsize);
	return out;
}

// Hash function to enable use of IDj as a map index for the memoization table
namespace std {
template <>
	struct hash <IDj>{
	public :
		size_t operator()(const IDj &x ) const;
	};
	size_t hash<IDj>::operator()(const IDj &x ) const
	{
	        size_t h = std::hash<uint64_t>()(x.t.l[0]) ^ std::hash<uint64_t>()((x.t.l[1]<<2) ^ ((uint64_t)x.score[0]<<50) ^ ((uint64_t)x.score[1]<<56));
		return  h;
	}
}



std::string joc::id2str(const IDj &a) {
	std::stringstream out;
	out << "A=" << (int)a.t.c[0] << " " << (int)a.t.c[1] << " " << (int)a.t.c[2] << " " <<  (int)a.t.c[3] << " " <<  (int)a.t.c[4] << " " << (int)a.t.c[5] << " (" << (int)a.score[0] << ")" << std::endl;
	out << "B=" << (int)a.t.c[8] << " " << (int)a.t.c[9] << " " << (int)a.t.c[10] << " " <<  (int)a.t.c[11] << " " <<  (int)a.t.c[12] << " " << (int)a.t.c[13] << " (" << (int)a.score[1] << ")" << std::endl;
	return out.str();
}
short joc::getMove() {
	return moviment;
}


IDj joc::getId(const signed char jug) {
	IDj id;
	for (int j=0; j<2; j++) {
		for (int i=0; i<6; i++) {
			id.t.c[i+j*8] = board[i][(jug+j)%2];
		}
	}
	id.score[0] = score[jug];
	id.score[1] = score[(jug+1)%2];
	return id;
}

bool joc::mou(short pos, signed char jug) {
	bool cangive = false;
	short fitxes = board[pos][jug];
	short fitxes_rival = 0;
	const signed char jug_ini = jug;
	const short pos_ini = pos;

	if (pos >= 6 || pos < 0 || fitxes <= 0) {
		return false;
	}

	for (int i=0; i<6; i++) {
		cangive = cangive || board[i][jug] > i;
	}

	board[pos][jug] = 0;
	
	while (fitxes > 0) {
		if (pos == 0) {
			pos = 5;
			jug = (jug+1)%2;
		}
		else {
			pos--;
		}

		if (pos != pos_ini || jug != jug_ini) {
			board[pos][jug]++;
			fitxes--;
		}
	}

	// Capture seeds 
	if (jug != jug_ini) {
		while (pos < 6 && (board[pos][jug] == 3 || board[pos][jug] == 2)) {
			score[jug_ini] += board[pos][jug];
			board[pos][jug] = 0;
			pos++;
		}
	}

	// Can't starve opponent 
	for (int i=0; i<6; i++) {
		fitxes_rival = fitxes_rival + board[i][(jug_ini+1)%2];
	}

	return !(fitxes_rival == 0 && cangive);
}

int joc::ia(const signed char jug, uint8_t rec, const uint8_t path) {
	int value = INT_MIN;
	long long int anti_val, valor_actual;
	short pos;
	
	IDj id = getId(jug);
#ifdef MEMOIZE_ENABLED
	assert(memoize);
	mtx.lock_shared();
	if (memoize->find(id) != memoize->end()) {
#ifdef DEBUG
		mtx2.lock();
		used_memoizes++;
		mtx2.unlock();
#endif
		if (((*memoize)[id]).r >= rec) {
			moviment = ((*memoize)[id]).m;
			auto retval = ((*memoize)[id]).v;
			mtx.unlock_shared();
			return retval;
		}
	}
	mtx.unlock_shared();
#endif

	valor_actual = getValue(jug);
	if (rec <= 0) {
		return valor_actual;
	}
	if (g_terminate != 0 && g_terminate != path) {
		return valor_actual;
	}

	if (valor_actual < PODA && rec < RECURSION_LEVEL) {
#ifdef DEBUG
		std::cout << "Poda realitzada" << std::endl;
#endif
		if (valor_actual - PODA_PENALTY < INT_MIN)
			return INT_MIN;
		else
			return valor_actual - PODA_PENALTY;
	}

	auto time = clock();
	if (g_clock_ticking && rec > MIN_RECURSION && (time-g_start_time)*1000/CLOCKS_PER_SEC > TIME_BUDGET_HINT) {
		rec--;
	} else if (g_clock_ticking && rec > 1 && (time-g_start_time)*1000/CLOCKS_PER_SEC > TIME_BUDGET_MAX) {
		rec--;
	}

	for (pos=0; pos < 6; pos++) {
		if (board[pos][jug] > 0) {
			joc *c = this->copy();
			if (! c->mou(pos, jug) ) {
				delete c;
				continue;
			}
			anti_val = c->ia((jug+1)%2, rec-1, path);
			int valor_actual_tmp = (valor_actual-anti_val>INT_MIN)? valor_actual-anti_val: INT_MIN;
			valor_actual = (valor_actual-anti_val<INT_MAX)? valor_actual_tmp: INT_MAX;
			if (valor_actual > value || moviment == -1) {
				moviment = pos;
				value = valor_actual;
			}
			delete c;
		}
	}
	
#ifdef VERBOSE_DEBUG
	if (rec > 0) {
		std::cout << "rec=" << rec << std::endl;
		print();
		for(int i=RECURSION_LEVEL-rec; i>0; i--) {
			std::cout << " ";
		}
		if (jug == ME)
			std::cout << "ME  ";
		else
			std::cout << "THEM ";
		std::cout << value << std::endl;
	}
	
	/*if (rec >= RECURSION_LEVEL-1) {
		if (jug == ME)
			std::cout << "ME  ";
		else
			std::cout << "THEM ";
		std::cout << "(" << moviment+1 <<") " << value << std::endl;
	}*/
#endif

	if (moviment == -1) {
		// We may not find a move because we can't move without starving. If so, do whatever, as we are forced to starve opponent.
		if (board[0][jug] > 0) {
			moviment = 0;
		}
		else if (board[1][jug] > 0) {
			moviment = 1;
		}
		else if (board[2][jug] > 0) {
			moviment = 2;
		}
		else if (board[3][jug] > 0) {
			moviment = 3;
		}
		else if (board[4][jug] > 0) {
			moviment = 4;
		}
		else if (board[5][jug] > 0) {
			moviment = 5;
		}
		else {
			// If we can't move, the opponent gets every remaining piece on the board. 
			value=value-(MORTES_MULT)*(48-score[ME]-score[THEM]);
		}
	}

#ifdef MEMOIZE_ENABLED
	assert(memoize);
	if (rec >= MIN_RECURSION)  {
		mtx.lock();
		if ((memoize->size() < MEMOIZE_MAX_SIZE)) {
			if ((memoize->find(id) == memoize->end()) || (memoize->find(id)->second).r < rec) {
				(*memoize)[id] = (memItem){ .v=value, .m=moviment, .r=rec };
			}
		}
		mtx.unlock();
	}
#endif

	return value;
}

joc* joc::copy() {
	joc *r = new joc();
	memcpy(r->board, this->board, 12*sizeof(signed char));
	r->moviment = -1;
	r->score[ME] = this->score[ME];
	r->score[THEM] = this->score[THEM];
	r->memoize = this->memoize;
	return r;
}
joc::~joc() {
}

int joc::getValue(const signed char jug) {
	const signed char altre = (jug+1)%2;
	int ret = 0;
	short i,j;
	int ventatja_fitxes = 0;
	int num_fitxes_jug = 0;
	int num_fitxes_altre = 0;

	for (j=0; j<2; j++) {
		for (i=0; i<6; i++) {
			// Heuristic 1: board ilegal 
			if (board[i][j] < 0) {
				return INT_MIN;
			}

			// Heuristic 2: 2 o 3 fitxes
			if (board[i][j] == 2 || board[i][j] == 1) {
				if (j == altre)
					ret += _12AWARD;
				else
					ret -= _12PENALTY;
			}

			// Heuristic 4: obligat a donar fitxes
			if (j == altre)
				num_fitxes_altre += board[i][j];
			else
				num_fitxes_jug += board[i][j];

			//Heuristic 8: espais buits
			if (j == jug && board[i][j] == 0)
				ret -= ESPAISBUITS_PENALTY;

			//Heuristic 9: Flow
			if (j == jug && i - board[i][j] > 0) {
				ret += FLOW_AWARD;
			}

			//Heuristic 10: 2 0 seguits
			if (i>0 && j==jug && board[i][j] == 0 && board[i-1][j] == 0) {
				ret -= DOBLEZERO_PENALTY;
			}

			// Heuristic 11: rival acumulant fitxes
			if (j == altre && board[i][j] > 11+i) {
				ret -= (board[i][j] - 11-i) * ACUM_PENALTY;
			}
		}
	}

	ret += score[jug] * MATADES_MULT;
	ret -= score[altre] * MORTES_MULT;

	// Heuristic 4. Si l'altre no pot moure, no es culpa nostra (controlat a mou())
	if (num_fitxes_altre == 0) {
		ret += num_fitxes_jug * MATADES_MULT;
	}

	// Heuristic 6: Partida perduda
	if (score[altre] >= 25)
		return (ret+INT_MIN/4 > INT_MIN)? ret+INT_MIN/4: INT_MIN+1;

	// Heuristic 7: Partida guanyada
	if (score[jug] >= 25)
		return (ret+INT_MAX/4 < INT_MAX)? ret+INT_MAX/4: INT_MAX-1;

	// Heuristic 3: mes fitxes que l'adversari
	ventatja_fitxes = num_fitxes_jug - num_fitxes_altre;
	ret += ventatja_fitxes * MESFITXES_MULT;

	return ret;
}

void joc::print() {
	int i;

#if BOARD_MODE == ME
/*	std::cout << "1 2 3 4 5 6" << std::endl;
	std::cout << "-----------" << std::endl;*/
	for (i=0; i<6; i++) {
		std::cout << (signed)board[i][THEM] <<" ";
	}
	std::cout << std::endl;
	for (i=5; i>=0; i--) {
		std::cout << (signed)board[i][ME] <<" ";
	}
#else
	for (i=0; i<6; i++) {
		std::cout << (signed)board[i][ME] <<" ";
	}
	std::cout << std::endl;
	for (i=5; i>=0; i--) {
		std::cout << (signed)board[i][THEM] <<" ";
	}
/*	std::cout << std::endl << "-----------";
	std::cout << std::endl << "6 5 4 3 2 1";*/
#endif
	std::cout << std::endl;
	std::cout << "Score: ME=" << (int)score[ME] << " THEM=" << (int)score[THEM] << std::endl;
}

void joc::ini() {
	short i,j;

	for (i=0; i<6; i++) {
		for (j=0; j<2; j++) {
			board[i][j] = 4;
		}
	}
	score[ME] = 0;
	score[THEM] = 0;
}


std::unordered_map<IDj, memItem >* load_memoize(std::string filename) {
	std::unordered_map<IDj, memItem > *ret = new std::unordered_map<IDj, memItem >;
	IDj key;
	memItem valmovrec;
	std::ifstream file;
	file.open(filename, std::ios::in |std::ios::binary);
	std::cout << "Loading from " << filename << "..." << std::endl;;
	while (!file.eof() && !file.fail()) {
		file >> key >> valmovrec;
		// easy fast check to detect data corruption
		if (valmovrec.m >= 0 && valmovrec.m < 6) {
			(*ret)[key] = valmovrec;
		}
	}
	std::cout << ret->size() << " values loaded." << std::endl;
	return ret;
}

int main(int argc, char**argv) {
	short pos;
	joc t;
	joc *spec[6];
	std::thread *th[6];
	std::string primer = "";
	clock_t time;
	short last_eviction=0; // indica quin quart de cache de memoització toca esborrar
	int c;
	char *memoize_file = NULL;

	MATADES_MULT = 0;
	MORTES_MULT = 0;
	while ((c = getopt (argc, argv, "p:a:d:m:")) != -1) {
		switch(c) {
			case 'p':
				if (!strcmp(optarg, "ME"))
					primer = "ME";
				else 
					primer = "THEM";
				break;
			case 'd':
				MORTES_MULT = atoi(optarg);
				break;
			case 'a':
				MATADES_MULT = atoi(optarg);
				break;
			case 'm':
				memoize_file = (char*)malloc(sizeof(optarg));
				strcpy(memoize_file, optarg);
		}
	}

	if (!memoize_file) {
		memoize_file = (char*)malloc(sizeof("memoize.dat"));
		strcpy(memoize_file, "memoize.dat");
	}
	
	srand(clock());
	if (MATADES_MULT == 0)
		MATADES_MULT = rand() % 150 + 250;
	if (MORTES_MULT == 0)
		MORTES_MULT = rand() % 150 + 250;


#ifdef DEBUG
	std::cout << "Agression level: " << MATADES_MULT << "/" << MORTES_MULT << std::endl << std::endl;
#endif

	t.ini();
#ifdef MEMOIZE_ENABLED
	t.memoize = load_memoize(memoize_file);
#ifdef VERBOSE_DEBUG
	std::cout << std::endl;
	for (auto it= t.memoize->begin(); it != t.memoize->end(); it++) {
		std::cout << t.id2str(it->first) << "\t\t\t\t-> " << it->second.v << "\t" << it->second.m << "\t" << it->second.r << std::endl; 
	}
#endif
#endif

	if (primer == "") {
		std::cout << "Who starts? [ME/THEM] ";
		std::cin >> primer;
	}
#if PRINT_MODE == MACHINE
	int pipe_r, pipe_w;
	const char *fifo1= "/tmp/awale1";
	const char *fifo2= "/tmp/awale2";
	signal(SIGPIPE, SIG_IGN);
	mkfifo(fifo1, 0666);
	mkfifo(fifo2, 0666);
	if (primer == "ME") {
		pipe_r = open(fifo1, O_RDONLY);
		pipe_w = open(fifo2, O_WRONLY);
	}
	else {
		pipe_w = open(fifo1, O_WRONLY);
		pipe_r = open(fifo2, O_RDONLY);
	}
	if (!pipe_r || !pipe_w)
		exit(1);
#endif


	if (primer == "ME") {
		g_clock_ticking = true;
		t.print();
		t.ia(ME, RECURSION_LEVEL);
		std::cout << "Move from ";
		std::cout << t.getMove()+1 << std::endl;
#if PRINT_MODE == MACHINE
		char buf[16];
		snprintf(buf, sizeof(buf), "%d", t.getMove()+1);
		ssize_t pipecheck = write(pipe_w, buf, sizeof(buf));
		if (pipecheck <= 0) {
			std::cout << "Pipe broken, exiting." << std::endl;
			exit(0);
		}
#endif
		if (!t.mou(t.getMove(), ME)) {
			std::cout << "Invalid move: " << (int)pos+1 << std::endl;
		}
		jugada++;
	}

	for(;;) {
		g_terminate = 0;
		g_clock_ticking = false;
#if PRINT_MODE == MACHINE
		if (t.score[ME] > 24 || t.score[THEM] > 24) {
			t.print();
			exit(0);
		}
#endif

		// Multithread while waiting for rival 
		for (int i=0; i<6; i++) {
			// Generate 6 games with each possible play by the opponent. Launch threaded execution.
			spec[i] = t.copy();
			if (spec[i]->mou(i, THEM)) {
				th[i] = new std::thread(&joc::ia, spec[i], ME, RECURSION_LEVEL, i+1);
			}
			else {
				// Illegal move, discard
				th[i] = NULL;
			}
		}

#ifdef MEMOIZE_ENABLED
		// save to a file
		std::ofstream myfile;
		myfile.open(memoize_file, std::ios::out| std::ios::binary);
		for (auto it = t.memoize->begin(); it != t.memoize->end(); it++) {
			IDj key;
			memItem valmovrec;
			key = it->first;
			valmovrec = it->second;
			myfile << key << valmovrec;
		}
		myfile.close();
#endif

		time = clock();

		t.print();
		std::cout << "Their move [1-6]: ";
#if PRINT_MODE == HUMAN
		std::cin >> pos;
#endif
#if PRINT_MODE == MACHINE
		char buf[16];
		ssize_t pipecheck = read(pipe_r, buf, 16);
		if (pipecheck <= 0) {
			std::cout << "Pipe broken, exiting." << std::endl;
			exit(0);
		}
		pos = atoi(buf);
		std::cout << pos << std::endl;
#endif
		bool ok = t.mou(pos-1, THEM);
		if (ok)  {
			jugada++;
			t.print();
			// Set every thread except 'pos' thread to be terminated.
			g_terminate = pos;
			g_start_time = clock();
			g_clock_ticking = true;
			// Join 'pos' thread. Kill the rest later.
			th[pos-1]->join();
			assert(t.mou(spec[pos-1]->getMove(), ME));
			g_clock_ticking = false;

			// End condition
			if (spec[pos-1]->getMove() == - 1) {
				int fitxes_fi = 0;
				std::cout << std::endl << "No more moves. Game is over." << std::endl;
				for (int i=0; i<6; i++) {
					fitxes_fi += t.board[i][THEM];
				}
				t.score[THEM] += fitxes_fi; 
				std::cout << "Score: ME=" << (int)(t.score[ME]) << " THEM=" << (int)t.score[THEM] << std::endl;
				return 0;
			}

			std::cout << "Move from ";
			std::cout << spec[pos-1]->getMove()+1 << std::endl;
#if PRINT_MODE == MACHINE
			char buf[16];
			snprintf(buf, sizeof(buf), "%d", spec[pos-1]->getMove()+1);
			ssize_t pipecheck = write(pipe_w, buf, sizeof(buf));
			if (pipecheck <= 0) {
				std::cout << "Pipe broken, exiting." << std::endl;
				exit(0);
			}
#endif
			jugada++;
			// Wait for the rest of the threads to continue 
			for (int i=pos%6; i!=pos-1; i=(i+1)%6) {
				if (th[i]) {
					th[i]->join();
				}
			}

			// Timekeeping 
			int ms = (clock()-g_start_time)*1000/CLOCKS_PER_SEC;
#ifdef DEBUG
			std::cout << "Computed in " << (clock()-time)*1000/CLOCKS_PER_SEC << " milliseconds. Total time vs player: " << ms << " milliseconds." << std:: endl;;
#ifdef MEMOIZE_ENABLED
			std::cout << "Memoize table size: " << t.memoize->size() << std:: endl;;
#endif
#endif
			if (ms < TIME_BUDGET_MIN) {
				RECURSION_LEVEL = MIN(RECURSION_LEVEL+1, MAX_RECURSION);
#ifdef DEBUG
				std::cout << "Increasing difficulty (" << RECURSION_LEVEL << ")" << std::endl;
#endif
			}
			if (ms > TIME_BUDGET_MAX) {
				RECURSION_LEVEL--;
#ifdef DEBUG
				std::cout << "Lowering difficulty (" << RECURSION_LEVEL << ")" << std::endl;
#endif
			}
#ifdef DEBUG
			std::cout << "Memoized values used: " << used_memoizes << std::endl;
#endif

#ifdef MEMOIZE_ENABLED
			// Delete memoization table items when the table is full. Randomly select 1/4 of the table for deletion.
			if (t.memoize->size() >= MEMOIZE_MAX_SIZE) {
#ifdef DEBUG
			std::cout << "Memoize table is full. Pruning " << used_memoizes << std::endl;
#endif
				auto quart = std::distance(t.memoize->begin(),t.memoize->end()) / 4;
				auto it1 = t.memoize->begin();
				auto it2 = t.memoize->begin();
				auto it3 = t.memoize->begin();
				switch (last_eviction) {
				case 0:
					std::advance(it1, quart);
					t.memoize->erase(t.memoize->begin(), it1);
					break;
				case 1:
					std::advance(it1, quart);
					std::advance(it2, quart*2);
					t.memoize->erase(it1, it2);
					break;
				case 2:
					std::advance(it2, quart*2);
					std::advance(it3, quart*3);
					t.memoize->erase(it2, it3);
					break;
				case 3:
					std::advance(it3, quart*3);
					t.memoize->erase(it3, t.memoize->end());
					break;
				}
				last_eviction = (last_eviction + 1) % 4;
			}
#endif
			// Speculative boards can be deleted now, as we will create them anew the next iteration.
			for (int i=0; i<6; i++) {
				if (spec[i]) {
					delete spec[i];
					spec[i] = NULL;
				}
			}
		}
	}
}
