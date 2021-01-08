#include <iostream>
#include "scan.h"
#include <vector>
#include <list>

int main()
try {
	std::cout << "Please enter I and U in such form: I = 10A, U = 5V" << std::endl;
	std::cout << "Input: ";
	std::string input;
	std::getline(std::cin, input);
	scn::scanner scan(input);

	double I, U;
	scan >> "I" >> scn::skip_spaces >> "=" >> scn::skip_spaces >> I >> "A";
	scan >> "," >> scn::skip_spaces;
	scan >> "U" >> scn::skip_spaces >> "=" >> scn::skip_spaces >> U >> "V;?";
	scan >> scn::skip_spaces >> scn::end_of_text;

	std::cout
		<< "\nresults:\n"
		<< "I = " << I << "A" << std::endl
		<< "U = " << U << "V" << std::endl
		<< "R = " << U / I << "Ohm" << std::endl;
	return 0;
} catch (scn::scan_error& e) {
	std::cerr << "error while parsing input data: " << e.what() << std::endl;
}