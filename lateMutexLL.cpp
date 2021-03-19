#include <cstdio> // for printf, no cout in multithreaded
#include <thread>
#include <mutex>
//#include <cstdint>
//#include <sstream>
#include <chrono>
#include <iostream>
//#include <set>
//#include <cstdint>
#include <sstream>
#include <chrono>
#include <atomic>
#include <stdlib.h>
#include <mutex>


//using std::cin;
//using std::cout;
//using std::cerr;
//using std::endl;
//using std::string;
//using std::stringstream;
//using std::set;
using std::atomic_compare_exchange_strong;
using std::atomic;
using std::cout;
using std::cin;
using std::endl;
using std::thread;
using std::mutex;

const uint64_t SIZE = 100000ULL;

const uint32_t numWorkUnits{ 100000 };
uint32_t currentWorkUnit{ 0 };

uint8_t* arr{ nullptr };
uint8_t* result{ 0 };
//uint64_t indexSlice = SIZE/numWorkUnits;
//uint64_t startIndex{0};
//uint64_t endIndex{0};

uint16_t numThreads{ 0 };

mutex mutexMyMutex;
atomic<int> cnt;
mutex atomicMyMutex;


//******************
//The MutexNode class
//******************
//template <typename T>
class MutexNode {
public:
	int data{};
	MutexNode* next{ nullptr };
};

//******************
// The linked list base class
//******************
////template <typename T>
class MutexLinkedList {
public:
	MutexLinkedList();
	~MutexLinkedList();

	bool mutFind(int key);
	bool mutDelete(int key);
	bool mutInsert(int key);

	MutexNode *first{ nullptr };
	MutexNode *last{ nullptr };
};
//
////template <typename T>
MutexLinkedList::MutexLinkedList() {

	// start off with a linked list of 5 MutexNodes
	//mutInsert(10);
	//mutInsert(20);
	//mutInsert(40);
	//mutInsert(60);
	//mutInsert(80);

};// constructor


//******************
//The AtomicNode class
//******************

class AtomicNode {
public:
	int data{};
	atomic<AtomicNode*> next;
};

//******************
// The linked list base class
//******************
class AtomicLinkedList {
public:
	AtomicLinkedList();

	bool atomFind(int key);
	bool atomDelete(int key);
	bool atomInsert(int key);
	AtomicNode* atomSearch(int key, AtomicNode **left_AtomicNode);

	AtomicNode *head;
	AtomicNode *tail;
};

//template <typename T>
AtomicLinkedList::AtomicLinkedList() {
	head = new AtomicNode();
	tail = new AtomicNode();
	head->next = tail;

	// start off with a linked list of 5 AtomicNodes
	atomInsert(55);
	atomInsert(10);
	atomInsert(20);
	atomInsert(5);
	atomInsert(40);
	atomInsert(97);
	atomInsert(60);
	atomInsert(80);

};// constructor



AtomicNode* getMarked(AtomicNode* ptr) {
	uint64_t val = reinterpret_cast<std::uintptr_t>(ptr);
	if (val % 2 == 0) {
		val += 1;
	}
	return reinterpret_cast<AtomicNode*>(val);
}

AtomicNode* getUnmarked(AtomicNode* ptr) {
	uint64_t val = reinterpret_cast<std::uintptr_t>(ptr);
	if (val % 2 == 1) {
		val -= 1;
	}
	return reinterpret_cast<AtomicNode*>(val);
}

bool isMarked(AtomicNode *ptr) {
	uint64_t val = reinterpret_cast<std::uintptr_t>(ptr);
	return (val % 2 == 1);
}

AtomicNode *AtomicLinkedList::atomSearch(int key, AtomicNode **left_AtomicNode) {
	AtomicNode *left_AtomicNode_next, *right_AtomicNode;// , *test_AtomicNode;

	do {
		AtomicNode *t = head;
		AtomicNode *t_next = head->next;
		do {
			if (!isMarked(t_next)) {
				(*left_AtomicNode) = t;
				left_AtomicNode_next = t_next;
			}
			t = getUnmarked(t_next);
			if (t == tail) {
				break;
			}
			t_next = t->next;
		} while (isMarked(t_next) || (t->data < key));
		right_AtomicNode = t;
		left_AtomicNode_next = t;

		//cout << "iterating with left_AtomicNode_next and right_AtomicNode" << endl;
		if (left_AtomicNode_next == right_AtomicNode) {
			if ((right_AtomicNode != tail) && isMarked(right_AtomicNode->next)) {
				// goto search_again
				continue;
			}
			else {
				//cout << "this is where we're returning" << endl;
				return right_AtomicNode;
			}
		}

		if (atomic_compare_exchange_strong(&((*left_AtomicNode)->next), &left_AtomicNode_next, right_AtomicNode)) {
			if ((right_AtomicNode != tail) && isMarked(right_AtomicNode->next)) {
				continue;
			}
			else {
				return right_AtomicNode;
			}
		}
	} while (true);
}


bool AtomicLinkedList::atomInsert(int key) {
	AtomicNode *atomicNewAtomicNode = new AtomicNode();
	atomicNewAtomicNode->data = key;
	AtomicNode *right_AtomicNode, *left_AtomicNode;

	do {
		//cout << "begin insert search" << endl;
		right_AtomicNode = atomSearch(key, &left_AtomicNode);
		if ((right_AtomicNode != tail) && (right_AtomicNode->data == key)) {
			return false;
		}
		atomicNewAtomicNode->next = right_AtomicNode;
		if (atomic_compare_exchange_strong(&(left_AtomicNode->next), &right_AtomicNode, atomicNewAtomicNode)) {
			return true;
		}
	} while (true);

}

bool AtomicLinkedList::atomDelete(int key) {
	AtomicNode *right_AtomicNode, *right_AtomicNode_next, *left_AtomicNode;

	do {
		right_AtomicNode = atomSearch(key, &left_AtomicNode);
		if ((right_AtomicNode == tail) || (right_AtomicNode->data != key)) {
			return false;
		}
		right_AtomicNode_next = right_AtomicNode->next;
		if (!isMarked(right_AtomicNode_next)) {
			if (atomic_compare_exchange_strong(&(right_AtomicNode->next), &right_AtomicNode_next, getMarked(right_AtomicNode_next))) {
				break;
			}
		}
	} while (true);
	if (!atomic_compare_exchange_strong(&(left_AtomicNode->next), &right_AtomicNode, right_AtomicNode_next)) {
		right_AtomicNode = atomSearch(right_AtomicNode->data, &left_AtomicNode);
	}
	return true;
}

bool AtomicLinkedList::atomFind(int key) {
	AtomicNode *right_AtomicNode, *left_AtomicNode;

	right_AtomicNode = atomSearch(key, &left_AtomicNode);
	if ((right_AtomicNode == tail) || (right_AtomicNode->data != key)) {
		return false;
	}
	else {
		return true;
	}
}

void atomicDoWork(uint8_t threadID, AtomicLinkedList atomicMyList) {
	int rand_1;
	int rand_2;
	srand(time(NULL));

	while (cnt < 100000) {
		rand_1 = rand() % 100;
		rand_2 = rand() % 10;


		if (rand_2 == 0) {

			if (cnt % 250 == 0) {
				cout << "Check in: deleting AtomicNode..." << endl;
			}

			atomicMyList.atomDelete(rand_1);
			cnt += 1;
			atomic_fetch_add(&cnt, 1);


		}
		else if (rand_2 == 1) {

			if (cnt % 250 == 0) {
				cout << "Check in: inserting AtomicNode..." << endl;
			}

			atomicMyList.atomInsert(rand_1);
			cnt += 1;
			atomic_fetch_add(&cnt, 1);


		}
		else {
			if (cnt % 250 == 0) {
				cout << "Check in: finding AtomicNode..." << endl;
			}

			atomicMyList.atomFind(rand_1);
			cnt += 1;
			atomic_fetch_add(&cnt, 1);
		}
		if (cnt % 1000 == 0) {
			cout << cnt << endl;
		}

	}
}



////template <typename T>
MutexLinkedList::~MutexLinkedList() {
	MutexNode* temp = first;
	while (temp) {
		first = first->next;
		delete temp;
		temp = first;
	}
}// destructor
//
//template <typename T>
bool MutexLinkedList::mutInsert(int key) {
	// do stuff
	// Need to do global lock. Simply lock first MutexNode with global mutex
	MutexNode* newMutexNode = new MutexNode();
	newMutexNode->data = key;
	if (!first) {
		first = newMutexNode;
		last = newMutexNode;
	}
	else if (newMutexNode->data < first->data) {
		newMutexNode->next = first;
		first = newMutexNode;
	}
	else {
		MutexNode* temp{ this->first };
		while (temp->data < key) {
			temp = temp->next;
			if (temp->next == NULL) {
				temp->next = newMutexNode;
				last = newMutexNode;
			}
		}
		newMutexNode->next = temp->next;
		temp->next = newMutexNode;
	}
	return true;
}
//
//template <typename T>
bool MutexLinkedList::mutDelete(int key) {
	// do stuff
	// Need to do global lock. Simply lock first MutexNode with global mutex
	if (!first) {
		return 0;
	}
	else {
		MutexNode* temp1{ this->first };
		MutexNode* temp2{ this->first };
		MutexNode* temp3{ this->first };
		while (temp1->data != key) {
			temp2 = temp1; // should make temp2 the MutexNode directly before temp1
			temp1 = temp1->next;
			temp3 = temp1->next;
			if (temp1->next == NULL) {
				return 0;
			}
		}
		temp1->next = temp3;
		delete temp2;
	}
	return true;
}
//
//template <typename T>
bool MutexLinkedList::mutFind(int key) { // is this enough for mutFind()?
	// do stuff
	MutexNode* temp{ this->first };
	if (!first) {
		return false;
	}
	else {
		while (temp->data != key) {
			temp = temp->next;
			if (temp->next = NULL) {
				return false;
			}
		}
		return true;
	}

}

void mutexDoWork(uint8_t threadID, MutexLinkedList mutexMyList) {//, MutexLinkedList mutexMyList) {//, int random1, int random2) {
	int rand_1;
	int rand_2;
	srand(time(NULL));
	rand_1 = rand() % 100;
	rand_2 = rand() % 10;
	uint64_t localResult = 0;
	uint64_t localWorkUnit{0};
	//printf("I am thread %u\n", threadID);

	uint64_t indexSlice = SIZE / numWorkUnits;
	uint64_t startIndex = 0;
	uint64_t endIndex = 0;

	while (currentWorkUnit < numWorkUnits) {
		int rand_1;
		int rand_2;
		srand(time(NULL));
		rand_1 = rand() % 100;
		rand_2 = rand() % 10;
		// MUTEX

		// rand_1 is the key
		// let rand_2 == 0 be insert()
		// let rand_2 == 1 be delete()
		// let rand_2 == 2 be find()
		if (rand_2 == 0) {
			mutexMyMutex.lock();
			// run critical region of code
			mutexMyList.mutInsert(rand_1);
			localWorkUnit = currentWorkUnit;
			currentWorkUnit++;
			result += localResult;
			mutexMyMutex.unlock();
		}
		else if (rand_2 == 1) {
			mutexMyMutex.lock();
			// run critical region of code
			mutexMyList.mutDelete(rand_1);
			localWorkUnit = currentWorkUnit;
			currentWorkUnit++;
			result += localResult;
			mutexMyMutex.unlock();
		}
		else {
			mutexMyMutex.lock();
			// run critical region of code
			localWorkUnit = currentWorkUnit;
			currentWorkUnit++;
			result += localResult;
			mutexMyMutex.unlock();
			mutexMyList.mutFind(rand_1);
		}

	}
}

int main() {
	int userChoice{ 0 };
	int numThreads{ 0 };
	cout << "Select (1) for Atomic or (2) for Mutex" << endl;
	cin >> userChoice;

	if (userChoice == 1) {
		cout << "How many threads would you like to run?" << endl;
		cin >> numThreads;
		numThreads = 8;
		AtomicLinkedList atomicMyList;
		AtomicNode *atomicNewAtomicNode = new AtomicNode();
		atomicNewAtomicNode = atomicMyList.head;
		auto start = std::chrono::high_resolution_clock::now();
		thread* threads = new thread[numThreads];


		for (int i = 0; i < numThreads; i++) {
			threads[i] = thread(atomicDoWork, i, atomicMyList);
		}
		for (int i = 0; i < numThreads; i++) {
			threads[i].join();
		}

		auto end = std::chrono::high_resolution_clock::now();
		auto timeInMilli = std::chrono::duration<double, std::milli>(end - start).count();
		printf("Master thread, child threads are complete! Total time: %1.3lf ms.\n", timeInMilli);

		while (atomicNewAtomicNode != atomicMyList.tail) {
			atomicNewAtomicNode = atomicNewAtomicNode->next;
			cout << atomicNewAtomicNode->data << endl;
		}
		return 0;
	}

	else {
		cout << "How many threads would you like to run?" << endl;
		cin >> numThreads;
		MutexLinkedList mutexMyList;
		numThreads = 1;

		// Create thread tracking objects:

		// need to make a list for these list actions to be carried out...
		auto start = std::chrono::high_resolution_clock::now();
		thread* threads = new thread[numThreads];

		// FORK-JOIN MODEL
		for (int i = 0; i < numThreads; i++) {
			threads[i] = thread(mutexDoWork, i, mutexMyList);// , rand_1, rand_2); // mutexDoWork tells these threads what to do.
		}
		for (int i = 0; i < numThreads; i++) {
			threads[i].join();
		}

		auto end = std::chrono::high_resolution_clock::now();
		auto timeInMilli = std::chrono::duration<double, std::milli>(end - start).count();
		printf("Master thread, child threads are complete! Total time: %1.3lf ms.\n", timeInMilli);


		delete[] threads;
		//delete[] arr;
		return 0;
	}

}