#include "vmath.hpp"



using namespace std;

int precedence(char op) {
    if (op == '+' || op == '-') return 1;
    if (op == '*' || op == '/') return 2;
    if (op == '^') return 3; 
    return 0;
}


double applyOperation(double a, double b, char op) {
    switch (op) {
    case '+': return a + b;
    case '-': return a - b;
    case '*': return a * b;
    case '/': return a / b;
    case '^': return pow(a, b); 
    }
    return 0;
}

double evaluateExpression(const string& expression) {
    stack<double> values; 
    stack<char> ops;

    for (size_t i = 0; i < expression.length(); i++) {
        if (isdigit(expression[i]) || expression[i] == '.') { 
            string number;
            while (i < expression.length() && (isdigit(expression[i]) || expression[i] == '.')) {
                number += expression[i];
                i++;
            }
            values.push(stod(number)); 
            i--; 
        }
        else if (expression[i] == '(') {
            ops.push(expression[i]);
        }
        else if (expression[i] == ')') {
            while (!ops.empty() && ops.top() != '(') {
                double b = values.top(); values.pop();
                double a = values.top(); values.pop();
                char op = ops.top(); ops.pop();
                values.push(applyOperation(a, b, op));
            }
            ops.pop(); 
        }
        else if (expression[i] == '+' || expression[i] == '-' ||
            expression[i] == '*' || expression[i] == '/' ||
            expression[i] == '^') { 
            while (!ops.empty() && precedence(ops.top()) >= precedence(expression[i])) {
                double b = values.top(); values.pop();
                double a = values.top(); values.pop();
                char op = ops.top(); ops.pop();
                values.push(applyOperation(a, b, op));
            }
            ops.push(expression[i]);
        }
    }

    while (!ops.empty()) {
        double b = values.top(); values.pop();
        double a = values.top(); values.pop();
        char op = ops.top(); ops.pop();
        values.push(applyOperation(a, b, op));
    }

    return values.top();
}

void replaceSubstring(string& str, const string& from, const string& to) {
    size_t start_pos = 0;
    while ((start_pos = str.find(from, start_pos)) != string::npos) {
        str.replace(start_pos, from.length(), to);
        start_pos += to.length(); 
    }
}

double evaluate_formula(string& expr) {
    //string expr = "2.5*(54-52)**2+6/2-2**2*10"; 
    replaceSubstring(expr, "**", "^"); 
    return evaluateExpression(expr);
}

///////////////////////////////





bool isOperator(char c) {
    return c == '+' || c == '-' || c == '*' || c == '/' || c == '^';
}


vector<string> infixToPostfix(const string& expr) {
    stack<char> ops;
    vector<string> postfix;
    string number;

    for (size_t i = 0; i < expr.length(); i++) {
        if (isdigit(expr[i]) || expr[i] == '.') { 
            number += expr[i];
        }
        else {
            if (!number.empty()) { 
                postfix.push_back(number);
                number.clear();
            }

            if (expr[i] == '(') {
                ops.push(expr[i]);
            }
            else if (expr[i] == ')') {
                while (!ops.empty() && ops.top() != '(') {
                    postfix.push_back(string(1, ops.top()));
                    ops.pop();
                }
                ops.pop(); 
            }
            else if (isOperator(expr[i])) {
                while (!ops.empty() && precedence(ops.top()) >= precedence(expr[i])) {
                    postfix.push_back(string(1, ops.top()));
                    ops.pop();
                }
                ops.push(expr[i]);
            }
        }
    }

    
    if (!number.empty()) {
        postfix.push_back(number);
    }

    
    while (!ops.empty()) {
        postfix.push_back(string(1, ops.top()));
        ops.pop();
    }

    return postfix;
}

double evaluatePostfix(const vector<string>& postfix) {
    stack<double> values;

    for (const string& token : postfix) {
        if (isdigit(token[0]) || (token[0] == '-' && token.length() > 1)) { 
            values.push(stod(token));
        }
        else { 
            double b = values.top(); values.pop();
            double a = values.top(); values.pop();
            switch (token[0]) {
            case '+': values.push(a + b); break;
            case '-': values.push(a - b); break;
            case '*': values.push(a * b); break;
            case '/': values.push(a / b); break;
            case '^': values.push(pow(a, b)); break;
            }
        }
    }

    return values.top(); 
}



double evaluate_formula_fp(string& expr) {
//    string expr = "2.5*(54-52)**2+6/2-2**2*10"; 
    replaceSubstring(expr, "**", "^"); 

    
    vector<string> postfix = infixToPostfix(expr);
  
    
    double result = evaluatePostfix(postfix);
    return result;
}

double factorial(double input) {
    // factorial nedefinit pentru numere negative
    if (input < 0.0)
        return NAN;

    // verificăm că input-ul este întreg (fără cast)
    if (fabs(input - floor(input)) > 1e-9)
        return NAN;

    // cazul de bază
    if (input == 0.0 || input == 1.0)
        return 1.0;

    // recursivitate
    return input * factorial(input - 1.0);
}