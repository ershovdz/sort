#include "stdafx.h"

//include STXXL 
#include <stxxl/io>
#include <stxxl/sort>
#include <stxxl/vector>

//include BOOST 
#include <boost/random/linear_congruential.hpp>
#include <boost/random/uniform_real.hpp>
#include <boost/random/variate_generator.hpp>

using namespace std;

const size_t NUM_ELEMENTS = 1024 * 1024 * 1024;

const size_t BLOCK_SIZE = sizeof(double) * 1024 * 1024;
const size_t RECORDS_IN_BLOCK = BLOCK_SIZE / sizeof(double);
const size_t NUM_BLOCKS = NUM_ELEMENTS / RECORDS_IN_BLOCK;
const size_t MEMORY_TO_USE = 256 * 1024 * 1024;  // memory for sorting 256 Mb

typedef stxxl::vector<double, 1, stxxl::lru_pager<8>, BLOCK_SIZE> vector_type;
typedef boost::minstd_rand base_generator_type;

class Managed_mem_block
{
public:
	Managed_mem_block(size_t block_size)
	{
		_block = stxxl::aligned_alloc<BLOCK_ALIGN>(block_size);
	}
	~Managed_mem_block() 
	{
		stxxl::aligned_dealloc<BLOCK_ALIGN>(_block); 
	}
	void* getBlock()
	{
		return _block;
	}
private:
	void* _block;
};

struct Cmp
{
	inline bool operator () (const double & a, const double & b) const
	{
		return a < b;
	}
	inline static double min_value()
	{
		return (std::numeric_limits<double>::min)();
	}
	inline static double max_value()
	{
		return (std::numeric_limits<double>::max)();
	}
};

void generateFile(const char* file_name)
{
	//create random numbers generator
	base_generator_type generator(42u);
	boost::uniform_real<> uni_dist(0,10000);
	boost::variate_generator<base_generator_type&, boost::uniform_real<> > uni(generator, uni_dist);

	if(file_name)
	{
		//create statistic object
		stxxl::stats_data stats_begin(*stxxl::stats::get_instance());
		double start = stxxl::timestamp();

		stxxl::syscall_file file(file_name, stxxl::file::CREAT | stxxl::file::RDWR | stxxl::file::DIRECT | stxxl::file::TRUNC);
		
		//allocate memory for a block
		Managed_mem_block block(BLOCK_SIZE);
		double * array = (double*)block.getBlock();

		for (size_t i = 0; i < NUM_BLOCKS; i++)
		{
			//populate block with random numbers and write it to disk 
			for (unsigned j = 0; j < RECORDS_IN_BLOCK; j++)
				array[j] = uni();

			stxxl::request_ptr req = file.awrite((void *)array
				, stxxl::int64(i) * BLOCK_SIZE
				, BLOCK_SIZE
				, stxxl::default_completion_handler());
			req->wait();
		}
		
		double stop = stxxl::timestamp();
		std::cout << stxxl::stats_data(*stxxl::stats::get_instance()) - stats_begin;
		std::cout << "File creation took " << (stop - start) << " seconds." << std::endl;
	}
}

void sortFile(const char* file_name)
{
	if(file_name)
	{
		stxxl::syscall_file file(file_name, stxxl::file::CREAT | stxxl::file::RDWR | stxxl::file::DIRECT);
		if(0 == file.size())
		{
			cout << "First generate a file, using create option..." << endl;
			return;
		}

		//create statistic object
		stxxl::stats_data stats_begin(*stxxl::stats::get_instance());
		double start = stxxl::timestamp();

		vector_type vec(&file);
		cout << "Vector size: " << vec.size() << endl;
		cout << "Checking order..." << endl;
		if(stxxl::is_sorted(vec.begin(), vec.end()))
		{
			cout << "Already sorted" << endl;
			return;
		}

		cout << "Sorting..." << endl;
		stxxl::sort(vec.begin(), vec.end(), Cmp(), MEMORY_TO_USE);

		double stop = stxxl::timestamp();
		std::cout << stxxl::stats_data(*stxxl::stats::get_instance()) - stats_begin;
		std::cout << "Sorting of "<< NUM_ELEMENTS << " elements took " << (stop - start) << " seconds." << std::endl;
	}
}

void checkOrder(const char* file_name)
{
	if(file_name)
	{
		stxxl::syscall_file file(file_name, stxxl::file::CREAT | stxxl::file::RDWR | stxxl::file::DIRECT);
		if(0 == file.size())
		{
			cout << "First create a file, using create option..." << endl;
			return;
		}

		vector_type vec(&file);
		cout << "Checking order..." << endl;
		if(stxxl::is_sorted(vec.begin(), vec.end()))
		{
			cout << "Already sorted" << endl;
		}
		else
		{
			cout << "Not sorted yet" << endl;
		}
	}
}

int main(int argc, char ** argv)
{
	if (argc < 3)
	{
		std::cout << "Usage: " << argv[0] << " action file_name" << std::endl;
		std::cout << "       where action is one of the follwing: create, sort, is_sorted" << std::endl;
		return -1;
	}

	if (strcmp(argv[1], "create") == 0) 
	{
		generateFile(argv[2]);
	} 
	else if (strcmp(argv[1], "sort") == 0)
	{
		sortFile(argv[2]);
	}
	else if(strcmp(argv[1], "is_sorted") == 0)
	{
		checkOrder(argv[2]);
	}
	else
	{
		cout << "Invalid action" << endl;
	}
	return 0;
}
