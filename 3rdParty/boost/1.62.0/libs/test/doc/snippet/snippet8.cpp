//  (C) Copyright Gennadiy Rozental 2001-2015.
//  Distributed under the Boost Software License, Version 1.0.
//  (See accompanying file LICENSE_1_0.txt or copy at 
//  http://www.boost.org/LICENSE_1_0.txt)
//
//  See http://www.boost.org/libs/test for the library home page.
//


//[snippet8
double find_root( double             (*f)(double), 
                  double               low_guess, 
                  double               high_guess, 
                  std::vector<double>& steps, 
                  double               tolerance )
{
    double solution;
    bool   converged = false;

    while(not converged)
    {
        double temp = (low_guess + high_guess) / 2.0;
        steps.push_back( temp );

        double f_temp = f(temp);
        double f_low = f(low_guess);
        
        if(abs(f_temp) < tolerance)
        {
            solution  = temp;
            converged = true;
        }
        else if(f_temp / abs(f_temp) == f_low / abs(f_low))
        {
            low_guess = temp;
            converged = false;
        }
        else
        {
            high_guess = temp;
            converged = false;
        }
    }
    
    return solution;
}
//]
