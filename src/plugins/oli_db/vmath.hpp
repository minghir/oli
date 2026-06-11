#ifndef VMATH_HPP
#define VMATH_HPP

#include <iostream>
#include <stack>
#include <string>
#include <cmath>
#include <cctype>
#include <vector>
#include <sstream>

#pragma once



double evaluateExpression(const std::string& expression);
void replaceSubstring(std::string& str, const std::string& from, const std::string& to);
std::vector<std::string> infixToPostfix(const std::string& expr);
double evaluatePostfix(const std::vector<std::string>& postfix);
double evaluate_formula_fp(std::string& expr);
double factorial(double input);

#endif