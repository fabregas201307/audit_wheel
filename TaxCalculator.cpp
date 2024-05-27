/*
tax calculator

then constructor is same as python version, refer to tax_calculator.py for its documentation for 
a list of functions you can call, please go to PYBIND11_MODEULE at the end of this file.

You can catch the errors raised here in python by normal menthods (e.g. except Exeption as e:)
*/

#include <ql/qldefines.hpp>
#if !defined() && defined(BOOST_MSVC)
#  include <ql/auto_link.hpp>
#endif
#include <ql/instruments/bonds/fixedratebond.hpp>
#include <ql/time/calendars/target.hpp>
#include <ql/time/calendars/unitedstates.hpp>
#include <ql/time/daycounters/actualactual.hpp>
#include <ql/time/daycounters/actual360.hpp>
#include <ql/time/daycounters/thirty360.hpp>

#include <iostream>
#include <vector>
#include <string>
#include <stdexcept>
#include <iomanip>
#include <map>
#include <cmath>

#include <pybind11/pybind11.h>
#include <pybind11/numpy.h>

#if PY_VERSION_HEX < 0x03000000
#define MyPyText_AsString PyString_AsString
#else
#define MyPyText_AsString PyUnicode_AsUTF8
#endif

using namespace QuantLib;
namespace py = pybind11;

class TaxCalculator {
    public:
        enum MarketDiscountMethod {MD_CONSTANT_YIELD, RATABLE_ACCRUAL};
        TaxCalculator(
            float coupon_rate,
            std::string date_start,
            std::string date_first_coupon,
            std::string date_purchase,
            std::string date_maturity,
            float price_issue,
            float price_purchase,
            float price_maturity,
            std::string cusip="",
            std::string day_count="30/360",
            std::string date_first_call_at_par="1901-01-01",
            int frequency=2,
            bool tax_exempt=false,
            bool include_MD_accrual_in_income=false,
            bool use_clean_price=false,
            bool use_adjusted_ytm=true,
            bool SIMULATE_KALOTAY=false,
            std::string market_discount_method="ratable_accrual",
            std::string acquisition_premium_method="scaled_oid",
        ){
            _cusip = cusip;

            if (coupon_rate < 0.0){
                throw_error("coupon_rate must be greater than or equal to 0.0, current value " + std::to_string(coupon_rate));
            } else if(coupon_rate == 0.0){
                use_adjusted_ytm = false; // TODO: improve this
            }
            _coupon_rate = coupon_rate;





        }