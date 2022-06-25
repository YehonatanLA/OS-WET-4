#include <unistd.h>
#include <string.h>
#include <iostream>
#include <cassert>
#include <sys/mman.h>
#include <sys/wait.h>
#include "malloc_3.cpp"
using namespace std;

#define MD_SIZE _size_meta_data()

// #define ASSERT_EQUAL(a,b)  \
// if(a != b){  \
// cout << "FAILED:" << endl << "\t\t" << #a << " is not equal to " << #b  << ", line: " << __LINE__ << endl << endl;    \
// exit(0); \
// }  

#define ASSERT_EQUAL(a,b,line)  \
if(a != b){  \
    cout << "FAILED:" << endl << "\t\t" << #a << " is not equal to " << #b  << ", file: " << __FILE__ << ", line: " << line << endl;    \
    cout << "\t\t" << "yours: " << a << ", expected: " << b << endl;  \
    exit(1); \
}  
#define PRINTFUNC(a)    \
    cout << a << endl;  \

// TODO: test 4,5,realloc - mmap

float current_test_num;

class Stats
{
    public:
    
    size_t num_free_blocks;
    size_t num_free_bytes;
    size_t num_allocated_blocks;
    size_t num_allocated_bytes;
    size_t num_meta_bytes;
    size_t size_meta_data;

    Stats();
    Stats(size_t a, size_t b, size_t c, size_t d, size_t e, size_t f);
    void set();
    void print();
    void setAndPrint();
    // bool compare(size_t a, size_t b, size_t c, size_t d, size_t e, size_t f);
    bool compare(size_t expected_num_free_blocks, size_t expected_num_free_bytes, size_t expected_num_allocated_blocks, 
                    size_t expected_num_allocated_bytes, size_t expected_num_meta_bytes, size_t expected_size_meta_data, int line);
};
Stats::Stats()
{
    num_free_blocks = 0;
    num_free_bytes = 0;
    num_allocated_blocks = 0;
    num_allocated_bytes = 0;
    num_meta_bytes = 0;
    size_meta_data = 0;
}
Stats::Stats(size_t a, size_t b, size_t c, size_t d, size_t e, size_t f)
{
    num_free_blocks = a;
    num_free_bytes = b;
    num_allocated_blocks = c;
    num_allocated_bytes = d;
    num_meta_bytes = e;
    size_meta_data = f;
};

void Stats::set()
{
    num_free_blocks = _num_free_blocks();
    num_free_bytes = _num_free_bytes();
    num_allocated_blocks =_num_allocated_blocks();
    num_allocated_bytes =_num_allocated_bytes();
    num_meta_bytes =_num_meta_data_bytes();
    size_meta_data =_size_meta_data();
};

void Stats::print()
{
    cout << "num_free_blocks = " << num_free_blocks << endl;
    cout << "num_free_bytes = " <<  num_free_bytes << endl;
    cout << "num_allocated_blocks = " << num_allocated_blocks  << endl;
    cout << "num_allocated_bytes = " <<  num_allocated_bytes << endl;
    cout << "num_meta_bytes = " <<  num_meta_bytes << endl;
    cout << "size_meta_data = " <<  size_meta_data << endl;
    cout << std::endl;
}

void Stats::setAndPrint()
{
    set();
    print();
}
   
bool Stats::compare(size_t expected_num_free_blocks, size_t expected_num_free_bytes, size_t expected_num_allocated_blocks, 
                    size_t expected_num_allocated_bytes, size_t expected_num_meta_bytes, size_t expected_size_meta_data, int line)
{
    ASSERT_EQUAL(num_free_blocks, expected_num_free_blocks, line);
    ASSERT_EQUAL(num_free_bytes, expected_num_free_bytes, line);
    ASSERT_EQUAL(num_allocated_blocks, expected_num_allocated_blocks, line);
    ASSERT_EQUAL(num_allocated_bytes, expected_num_allocated_bytes, line);
    ASSERT_EQUAL(num_meta_bytes, expected_num_meta_bytes, line);
    ASSERT_EQUAL(size_meta_data, expected_size_meta_data, line);
}
// bool Stats::compare(size_t expected_num_free_blocks, size_t expected_num_free_bytes, size_t expected_num_allocated_blocks, 
//                     size_t expected_num_allocated_bytes, size_t expected_num_meta_bytes, size_t expected_size_meta_data)
// {
//     ASSERT_EQUAL(num_free_blocks, expected_num_free_blocks);
//     ASSERT_EQUAL(num_free_bytes, expected_num_free_bytes);
//     ASSERT_EQUAL(num_allocated_blocks, expected_num_allocated_blocks);
//     ASSERT_EQUAL(num_allocated_bytes, expected_num_allocated_bytes);
//     ASSERT_EQUAL(num_meta_bytes, expected_num_meta_bytes);
//     ASSERT_EQUAL(size_meta_data, expected_size_meta_data);
// }

size_t align(size_t a)
{
    a=((a+7)/8)*8;
    return a;
}

void test_challenge_1()
{
    current_test_num = 1;
    // cout << "running test_challenge " << current_test_num << "...";
    // num_free_blocks
    // num_free_bytes
    // num_allocated_blocks // free and used
    // num_allocated_bytes // free and used
    // num_meta_bytes // num_allocated_blocks * MD_SIZE
    // size_meta_data // MD_SIZE

    Stats s;
    //test head and last
    void* p1 = smalloc(3000); // [40,3000,al] ->
    sfree(p1); // [40,3000,f] ->

    void* p2 = smalloc(32); // [40,32,al][40,2928,f] ->
    s.set();
    size_t expected_num_free_bytes = 3000 - 32 - 1*MD_SIZE;
    size_t expected_num_allocated_bytes = 32 + expected_num_free_bytes;
    s.compare(1,expected_num_free_bytes,2,expected_num_allocated_bytes,2*MD_SIZE,MD_SIZE,__LINE__);

    // test last
    void* p3 = smalloc(1000);
    s.set(); // [40,32,al][40,1000,al][40,1888,f]
    expected_num_free_bytes = expected_num_free_bytes - 1000 - 1*MD_SIZE;
    expected_num_allocated_bytes = expected_num_allocated_bytes - 1*MD_SIZE;
    s.compare(1,expected_num_free_bytes,3,expected_num_allocated_bytes,3*MD_SIZE,MD_SIZE,__LINE__);

    // test middle
    void* p4 = smalloc(1000); // [40,32,al][40,848,f][40,1000,al][40,1000,al]
    s.set();
    expected_num_free_bytes = expected_num_free_bytes - 1000 - 1*MD_SIZE;
    expected_num_allocated_bytes = expected_num_allocated_bytes - 1*MD_SIZE;
    s.compare(1,expected_num_free_bytes,4,expected_num_allocated_bytes,4*MD_SIZE,MD_SIZE,__LINE__);

    void* p5 = smalloc(504); // [40,32,al][40,304,f][40,504,al][40,1000,al][40,1000,al]

    // ********* this one fails for variant metadata sizes *********
    s.set();
    expected_num_free_bytes = expected_num_free_bytes - 504 - 1*MD_SIZE;
    expected_num_allocated_bytes = expected_num_allocated_bytes - 1*MD_SIZE;
    s.compare(1,expected_num_free_bytes,5,expected_num_allocated_bytes,5*MD_SIZE,MD_SIZE,__LINE__);
}
void test_challenge_1_2()
{   //comments represents actual memory
    current_test_num = 1.2;
    // cout << "running test_challenge " << current_test_num << "...";

    Stats s;

    void* p1 = smalloc(1000);
    void* p2 = smalloc(1000);
    void* p3 = smalloc(1000);
    sfree(p3); // [40,1000,al][40,1000,f][40,1000,al]

    void* p4 = smalloc(992); // [40,1000,al][40,1000,a][40,1000,al]

    s.set();
    size_t expected_num_free_bytes = 0;
    size_t expected_num_allocated_bytes = 3000;
    s.compare(0,expected_num_free_bytes,3,expected_num_allocated_bytes,3*MD_SIZE,MD_SIZE,__LINE__);

    sfree(p4); // [40,1000,al][40,1000,f][40,1000,al]

    s.set();
    s.compare(1,1000,3,3000,3*MD_SIZE,MD_SIZE,__LINE__);
}
void test_challenge_1_3()
{   //comments represents actual memory
    current_test_num = 1.3;
    // cout << "running test_challenge " << current_test_num << "...";
    
    Stats s;

    void* p1 = smalloc(1000);
    void* p2 = smalloc(1000);
    void* p3 = smalloc(1000);
    sfree(p3); // [40,1000,al][40,1000,f][40,1000,al]
    void* p4 = smalloc(1000-MD_SIZE-128); // [40,1000,al][40,1000,al][40,832,al][40,128,f]
    s.set();
    size_t expected_num_free_bytes = 128;
    size_t expected_num_allocated_bytes = 1000 + 1000 + (1000 - MD_SIZE - 128) + 128; // = 3000 - MD_SIZE
    s.compare(1,expected_num_free_bytes,4,expected_num_allocated_bytes,4*MD_SIZE,MD_SIZE,__LINE__);
}
void test_challenge_1_4()
{   //comments represents actual memory
    current_test_num = 1.4;
    // cout << "running test_challenge " << current_test_num << "...";
    
    Stats s;

    void* p1 = smalloc(1000);
    void* p2 = smalloc(1000);
    void* p3 = smalloc(1000);
    sfree(p3); // [40,1000,al][40,1000,f][40,1000,al]
    void* p4 = smalloc(1000-MD_SIZE-127); // [40,1000,al][40,1000,al][40,1000,al]
    s.set();
    size_t expected_num_free_bytes = 0;
    size_t expected_num_allocated_bytes = 3000;
    s.compare(0,expected_num_free_bytes,3,expected_num_allocated_bytes,3*MD_SIZE,MD_SIZE,__LINE__);
}
void test_challenge_2()
{   //comments represents sorted list
    current_test_num = 2;
    // cout << "running test_challenge " << current_test_num << "...";
    
    Stats s;

    int al_size = 10000;

    void* p1 = smalloc(1000);//
    void* p2 = smalloc(1000);//
    void* p3 = smalloc(1000);
    void* p4 = smalloc(1000);//
    void* p5 = smalloc(1000);   //
    void* p6 = smalloc(1000);//
    void* p7 = smalloc(1000);
    void* p8 = smalloc(1000);
    void* p9 = smalloc(1000);//
    void* p10 = smalloc(1000);//

    sfree(p1);
    sfree(p2); // [40,1000,al][40,1000,al]...[40,1000,al][40,2040,f]
    s.set();
    s.compare(1,2000+1*MD_SIZE,9,al_size+1*MD_SIZE,9*MD_SIZE,MD_SIZE,__LINE__);

    sfree(p4);
    sfree(p6);//       p3          p7         p8          p9         p10        p1+p2    p4+p5+p6
    sfree(p5);// [40,1000,al][40,1000,al][40,1000,al][40,1000,al][40,1000,al][40,2040,f][40,3080,f]
    s.set();
    s.compare(2,5000+3*MD_SIZE,7,al_size+3*MD_SIZE,7*MD_SIZE,MD_SIZE,__LINE__);

    sfree(p9);//       p3          p7         p8         p1+p2     p9+p10    p4+p5+p6
    sfree(p10);// [40,1000,al][40,1000,al][40,1000,al][40,2040,f][40,2040,f][40,3080,f]
    s.set();
    s.compare(3,7000+4*MD_SIZE,6,al_size+4*MD_SIZE,6*MD_SIZE,MD_SIZE,__LINE__);

    sfree(p7);//       p3       p1+p2    p4+..+p10
    sfree(p8);// [40,1000,al][40,2040,f][40,7240,f]
    s.set();
    s.compare(2,9000+7*MD_SIZE,3,al_size+7*MD_SIZE,3*MD_SIZE,MD_SIZE,__LINE__);

    sfree(p3);
    s.set();
    s.compare(1,10000+9*MD_SIZE,1,al_size+9*MD_SIZE,1*MD_SIZE,MD_SIZE,__LINE__);

}
void test_challenge_3_1()
{   //comments represents actual memory
    current_test_num = 3.1;
    // cout << "running test_challenge " << current_test_num << "...";
    
    Stats s;

    void* p1 = smalloc(2000);
    void* p2 = smalloc(4000);
    void* p3 = smalloc(3000);//      p1         p2          p3
    sfree(p3); //               [40,2000,al][40,4000,al][40,3000,f]

    s.set();
    s.compare(1,3000,3,9000,3*MD_SIZE,MD_SIZE,__LINE__);

    void* p4 = smalloc(1000); // [40,2000,al][40,4000,al][40,1000,al][40,1960,f]
    s.set();
    size_t expected_num_free_bytes = 3000-1000-MD_SIZE;
    size_t expected_num_allocated_bytes = 7000 + 3000-1000-MD_SIZE;
    s.compare(1,expected_num_free_bytes,4,expected_num_allocated_bytes,4*MD_SIZE,MD_SIZE,__LINE__);

    void* p5 = smalloc(7000); // [40,2000,al][40,4000,al][40,1000,al][40,7000,al]
    s.set();
    s.compare(0,0,4,14000,4*MD_SIZE,MD_SIZE,__LINE__);
}
void test_challenge_3_2() // just an edge case when the wilderness cunk is the only alloced memory
{   //comments represents actual memory
    current_test_num = 3.2;
    // cout << "running test_challenge " << current_test_num << "...";
    
    Stats s;

    void* p1 = smalloc(3000);
    sfree(p1); //                     p2
    void* p2 = smalloc(10000);// [40,10000,al]

    s.set();
    s.compare(0,0,1,10000,1*MD_SIZE,MD_SIZE,__LINE__);
}
// void test_challenge_4()
// {
//     current_test_num = 4;
//     cout << "running test_challenge " << current_test_num << "...";

//     int x=128*1024;
//     void* p1 = smalloc(x*2);
//     void* p2 = smalloc(x*3);
//     void* p3 = smalloc(500000);

//     srealloc(p2,800000);
// }
// void test_challenge_5()
// { // 40 to hex = 28
//     void* p1 = smalloc(25);
//     void* p2 = smalloc(25);
//     void* p3 = smalloc(25);
//     cout << p1 << endl;
//     cout << (size_t)p2 - (size_t)p1 << endl;
//     cout << p2 << endl;
//     cout << p3 << endl;
// }
// void test_alignment()
// {
//     int size = 733;
//     int *arr[size];
//     for(int i = 0; i < size; i++)
//     {
//         arr[i] = (int*)smalloc(2531*(size_t)(i+1));
//     }

//     bool mul_of_8;
//     bool all_mull_of_8 = true;
//     for(int i = 0; i < size; i++)
//     {
//         mul_of_8 = false;
//         cout << "arr[" << i << "] = " << arr[i] << endl;
//         mul_of_8 = (size_t)arr[i] % 8 == 0? true : false;
//         if(!mul_of_8)
//             all_mull_of_8 = false;
//         //cout << "is it mul of 8? " << mul_of_8 << endl << endl;
//     }
//     cout << "all mull  of 8? " << all_mull_of_8 << endl;
    
//     // mul_of_8 = (size_t)p1 % 8 == 0? true : false;
//     // cout << "is it mul of 8? " << mul_of_8 << endl;
//     // mul_of_8 = ((size_t)p2 - (size_t)p1 )%8 == 0? true : false;
//     // cout << mul_of_8 << endl;
//     // cout << p2 << endl;
//     // cout << p3 << endl;
// }

void test_srealloc_common_sizes_a()
{   //comments represents actual memory
    //current_test_num = 3.1;
    // cout << "running test_srealloc_common_sizes_a...";
    // 
    Stats s;

    void* p1 = smalloc(5000);
    void* p2 = smalloc(5000);
    void* p3 = smalloc(5000);
    void* p4 = smalloc(5000);
    void* p5 = smalloc(5000);
    void* p6 = smalloc(5000);
    void* p7 = smalloc(5000);
    void* p8 = smalloc(5000);
    void* p9 = smalloc(5000);
    void* p10 = smalloc(5000);

    sfree(p1);
    // printMemory();
    void* p11 = srealloc(p2, 2000); // no merging
    // printMemory();
    s.set();
    size_t expected_num_free_bytes = 5000 + 5000 - 2000 - MD_SIZE;
    size_t expected_num_allocated_bytes = 5000 * 9 + 2000 + 5000 - 2000 - MD_SIZE;
    s.compare(2,expected_num_free_bytes,11,expected_num_allocated_bytes,11*MD_SIZE,MD_SIZE,__LINE__);

    sfree(p10);
    sfree(p8);
    // printMemory();
    void* p12 = srealloc(p9, 2000); // a - no merging, split after useing the block. (should merge 2 free block afterwards?)
    // printMemory();
    expected_num_free_bytes = expected_num_free_bytes + 5000 + 8000;
    expected_num_allocated_bytes = expected_num_allocated_bytes;
    s.set();
    s.compare(4,expected_num_free_bytes,11,expected_num_allocated_bytes,11*MD_SIZE,MD_SIZE,__LINE__);
    
    smalloc(8000);
    p12 = srealloc(p9, 6000); // b, p9 isnt wilderness - merge with lower address (p8).
    expected_num_free_bytes = expected_num_free_bytes - 8000 - 5000 + 1000;
    expected_num_allocated_bytes = expected_num_allocated_bytes ;
    s.set();
    s.compare(3,expected_num_free_bytes,11,expected_num_allocated_bytes,11*MD_SIZE,MD_SIZE,__LINE__);

}
void test_srealloc_common_sizes_b1()
{   //comments represents actual memory
    // cout << "running test_srealloc_common_sizes_b1...";
    // 
    Stats s;

    void* p9 = smalloc(5000);
    void* p10 = smalloc(5000);

    sfree(p9);
    void* p11 = srealloc(p10, 20000); // b, p9 is wilderness - merge with lower address, and enlarge
    s.set();
    s.compare(0,0,1,20000,1*MD_SIZE,MD_SIZE,__LINE__);
}
void test_srealloc_common_sizes_b2()
{   //comments represents actual memory
    // cout << "running test_srealloc_common_sizes_b2...";
    
    Stats s;

    void* p9 = smalloc(5000);
    void* p10 = smalloc(5000);

    sfree(p9);
    void* p11 = srealloc(p10, 6000); // b, p9 is wilderness - merge with lower address, and dont enlarge - need to split afterwards
    s.set();
    s.compare(1,4000,2,10000,2*MD_SIZE,MD_SIZE,__LINE__);
}
void test_srealloc_common_sizes_c()
{   //comments represents actual memory
    // cout << "running test_srealloc_common_sizes_c...";
    
    Stats s;

    void* p1 = smalloc(5000);
    void* p2 = smalloc(5000);
    void* p3 = smalloc(5000);

    void* p4 = srealloc(p3, 20000); // c, no adjacent blocks, p3 is wilderness - enlarge
    s.set();
    s.compare(0,0,3,30000,3*MD_SIZE,MD_SIZE,__LINE__);
}
void test_srealloc_common_sizes_d()
{   //comments represents actual memory
    // cout << "running test_srealloc_common_sizes_d...";

    Stats s;

    void* p1 = smalloc(1000);
    void* p2 = smalloc(5000);
    void* p3 = smalloc(5000);
    sfree(p1);
    sfree(p3);
    void* p4 = srealloc(p2,7000); // d, lower and higher adjacent blocks, lower isnt enough, higher is.
    s.set();
    s.compare(2,1000 + 5000 - 2000,3,11000,3*MD_SIZE,MD_SIZE,__LINE__);
}
void test_srealloc_common_sizes_e()
{   //comments represents actual memory
    // cout << "running test_srealloc_common_sizes_e...";
    // int* a = (int*)smalloc(sizeof(int));
    // int i = 0;
    // while(1)
    // {
    //     a[i] = 1;
    //     i++;
    // }

    Stats s;

    void* p1 = smalloc(5000);
    void* p2 = smalloc(5000);
    void* p3 = smalloc(5000);
    sfree(p1);
    sfree(p3);
    void* p4 = srealloc(p2, 15000 + 2*MD_SIZE); // e, lower and higher adjacent blocks, join them all
    s.set();
    s.compare(0,0,1,15000 + 2*MD_SIZE,1*MD_SIZE,MD_SIZE,__LINE__);
}
void test_srealloc_common_sizes_e2()
{   //comments represents actual memory
    // cout << "running test_srealloc_common_sizes_e2...";

    Stats s;

    void* p1 = smalloc(5000);
    void* p2 = smalloc(5000);
    void* p3 = smalloc(5000);
    sfree(p1);
    sfree(p3);
    void* p4 = srealloc(p2, 13000); // e, lower and higher adjacent blocks, join them all, and dont split the free space to a second block 
    s.set();
    s.compare(0,0,1,15000 + 2*MD_SIZE,1*MD_SIZE,MD_SIZE,__LINE__);
}
void test_srealloc_common_sizes_f_i()
{   //comments represents actual memory
    // cout << "running test_srealloc_common_sizes_f_i...";

    Stats s;

    void* p1 = smalloc(5000);
    void* p2 = smalloc(5000);
    void* p3 = smalloc(5000);
    sfree(p1);
    sfree(p3);
    void* p4 = srealloc(p2, 20000); // f, lower and higher adjacent blocks, cant join them all (not enough space), higer is wilderness - so enlarge
    s.set();
    s.compare(0,0,1,20000,1*MD_SIZE,MD_SIZE,__LINE__);
}
void test_srealloc_common_sizes_f_ii()
{   //comments represents actual memory
    // cout << "running test_srealloc_common_sizes_f_ii...";

    Stats s;

    void* p0 = smalloc(5000);
    void* p1 = smalloc(5000);
    void* p2 = smalloc(5000);
    void* p3 = smalloc(5000);
    sfree(p3);

    void* p4 = srealloc(p2, 20000); // f, only higher adjacent block, cant join them (not enough space), higer is wilderness, enlarge
    // p0 p1 p4(2)
    s.set();
    s.compare(0,0,3,30000,3*MD_SIZE,MD_SIZE,__LINE__);

    sfree(p4);

    void* p5 = srealloc(p1,30000); // f, only higher adjacent block, can join them, higer is wilderness, enlarge
    s.set();
    s.compare(0,0,2,35000,2*MD_SIZE,MD_SIZE,__LINE__);
}
void test_srealloc_common_sizes_g()
{   //comments represents actual memory
    // cout << "running test_srealloc_common_sizes_g...";

    Stats s;

    void* p1 = smalloc(5000);
    void* p2 = smalloc(5000);
    void* p3 = smalloc(5000);
    void* p10 = smalloc(5000);
    void* p4 = smalloc(20000);
    void* p5 = smalloc(20000);

    sfree(p1);
    // sfree(p2);
    sfree(p3);
    void* p6 = srealloc(p2, 30000);

    s.set();
    s.compare(1,15000 + 2*MD_SIZE,5,5000 + 30000 + 2*20000 + 15000 + 2*MD_SIZE, 5*MD_SIZE,MD_SIZE,__LINE__);

    sfree(p6);

    void *p7 = srealloc(p10, 50000);
    s.set();
    s.compare(1,20000+ 3*MD_SIZE,4,50000 + 2*20000 + 15000 + 5000 + 3*MD_SIZE, 4*MD_SIZE,MD_SIZE,__LINE__);

}
#define NUM_TESTS 17

void (*tests[NUM_TESTS])(void) = {
	test_challenge_1,
	test_challenge_1_2,
	test_challenge_1_3,
    test_challenge_1_4,
	test_challenge_2,
    test_challenge_3_1,
    test_challenge_3_2,
    test_srealloc_common_sizes_a,
    test_srealloc_common_sizes_b1,
    test_srealloc_common_sizes_b2,
    test_srealloc_common_sizes_c,
    test_srealloc_common_sizes_d,
    test_srealloc_common_sizes_e,
    test_srealloc_common_sizes_e2,
    test_srealloc_common_sizes_f_i,
    test_srealloc_common_sizes_f_ii,
    test_srealloc_common_sizes_g
};
string tests_string[NUM_TESTS] = {
	"test_challenge_1",
	"test_challenge_1_2",
	"test_challenge_1_3",
    "test_challenge_1_4",
	"test_challenge_2",
    "test_challenge_3_1",
    "test_challenge_3_2",
    "test_srealloc_common_sizes_a",
    "test_srealloc_common_sizes_b1",
    "test_srealloc_common_sizes_b2",
    "test_srealloc_common_sizes_c",
    "test_srealloc_common_sizes_d",
    "test_srealloc_common_sizes_e",
    "test_srealloc_common_sizes_e2",
    "test_srealloc_common_sizes_f_i",
    "test_srealloc_common_sizes_f_ii",
    "test_srealloc_common_sizes_g"
};

int main(int argc,char* argv[]) {

    for(int i = 0; i < NUM_TESTS; i++)
    {
        cout << "running " << tests_string[i] << "... ";
        fflush(stdout);

        // time out shit
        struct timespec timeout;
        timeout.tv_sec = 5;
        timeout.tv_nsec = 0;
        siginfo_t sig_info;
        sigset_t mask;
        sigset_t orig_mask;
        sigemptyset (&mask);
        sigaddset (&mask, SIGCHLD);
        if (sigprocmask(SIG_BLOCK, &mask, &orig_mask) < 0) {
            perror ("sigprocmask");
            return 1;
        }

        //fork and run the next test
        pid_t pid = fork();
        if(pid == 0) // child
        {
            tests[i]();
            struct timespec remaining, request = {0, 100000000};
            nanosleep(&request, &remaining);
            exit(0);
        }
        else
        {
            // more timeout shit
            int sig = sigtimedwait(&mask, &sig_info, &timeout);
            if (sig < 0) {
                if (errno == EINTR) {
                    // Interrupted by a signal other than SIGCHLD.
                    continue;
                } else if (errno == EAGAIN) {
                    cout << " Timeout. ";
                    kill (pid, SIGKILL);
                } else {
                    //printf ("(%ld) sigtimedwait\n", time(0));
                    return 1;
                }
            }

            // wait for the test to finish and identify exit status
            int status;
            wait(&status);
            if(WIFEXITED(status))
            {
                int exit_code = WEXITSTATUS(status);
                if(exit_code == 0)
                {
                    cout << "PASSED." << endl;
                }
                else if(WEXITSTATUS(status) != 1)
                {
                    cout << "FAILED with exit status: " << exit_code << "." << endl;
                }
            }
            else if(WIFSIGNALED(status))
            {
                int sig_code = WTERMSIG(status);
                char* sig = strsignal(sig_code);
                cout << "FAILED with a signal: " << sig << endl;
            }
                
        }
	}


    // test_srealloc_common_sizes_a();
    
    return 0;
}
