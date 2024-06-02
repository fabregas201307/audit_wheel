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

            // process dates
            _date_start = get_ql_date(date_start);
            _date_first_coupon = get_ql_date(date_first_coupon);
            _date_purchase = get_ql_date(date_purchase);
            _date_maturity = get_ql_date(date_maturity);
            _date_first_call_at_par = get_ql_date(date_first_call_at_par);
            std::function<void(Date, const std::string&)> date_check = [&](Date date, const std::string& msg){
                if (_date == NIL_DATE) {return;}
                if (!(_date_start <= _date) || !(_date <= _date_maturity)){
                    throw_error(msg + ": date error. start date" + date_start + " maturity date " + date_maturity);
                }
            };
            date_check(_date_first_coupon, "first coupon date " + date_first_coupon);
            date_check(_date_purchase, "purchase date " + date_purchase);
            date_check(_date_first_call_at_par, "first par call date " + date_first_call_at_par);

            // process prices
            // normalize price to 100. ql ytm is not correct if par is not 100. TODO: remove this
            std::function<void(float, const std::string&)> price_check = [&](float price, const std::string& msg){
                if (price < 0.0){
                    throw_error(msg + " must be greater than or equal to 0.0, current value " + std::to_string(price));
                }
            };
            price_check(price_issue, "issue price " + std::to_string(price_issue));
            price_check(price_purchase, "purchase price " + std::to_string(price_purchase));
            price_check(price_maturity, "maturity price " + std::to_string(price_maturity));
            _price_mult = 100.0 / price_maturity;
            _price_issue = price_issue * _price_mult;
            _price_purchase = price_purchase * _price_mult;
            _price_maturity = 100.0;

            if (day_count == "30/360"){
                _day_count = Thirty360(Thirty360::ISMA);  // please make sure c++ quantlib version is not too old.  old versions should use Thirty360() instead.
            } else if (day_count == "ACT/ACT"){
                _day_count = ActualActual(ActualActual::ISMA);
            } else if (day_count == "actual/actual"){
                _day_count = ActualActual();
            } else {
                throw_error("day_count only supports '30/360' or 'ACT/ACT' .");
            }

            _frequency = static_cast<Frequency>(frequency);
            _tax_exempt = tax_exempt;
            _include_MD_accrual_in_income = include_MD_accrual_in_income;
            _use_clean_price = use_clean_price;
            _use_adjusted_ytm = use_adjusted_ytm;
            _SIMULATE_KALOTAY = SIMULATE_KALOTAY;
            set_market_discount_method(market_discount_method);
            set_acquisition_premium_method(acquisition_premium_method);
            _yield_from_issue_dirty = true;
            _yield_from_issue = 0.0;
            _yield_from_purchase_dirty = true;
            _yield_from_purchase = 0.0;
            _couponflow_dirty = true;

            // check for first call date
            if((_price_purchase > _price_maturity) && (_date_first_call_at_par != NIL_DATE)){
                _date_maturity = _date_first_call_at_par;
            }

            // create schedule and bond
            _schedule = std::unique_ptr<Schedule>(new Schedule(
                _date_start,
                _date_maturity,
                Period(_frequency),
                UnitedStates(UnitedStates::GovernmentBond),
                Unadjusted,
                Unadjusted,
                DateGeneration::Forward,
                false,
                _date_first_coupon
            ));
            _bond = std::unique_ptr<FixedRateBond>(new FixedRateBond(
                static_cast<Natural>(0), // settlement days
                static_cast<Real>(_price_maturity),
                *_schedule,
                std::vector<Rate>(1, _coupon_rate),
                _day_count,
                Unadjusted
            ));
        }

        Date get_ql_date(const std::string& date){
            if ((int)date.size() != 10) || (date[4] != '-') || (date[7] != '-'){
                throw_error("date must be in the format 'YYYY-MM-DD', current value " + date);
            }
            Year year = static_cast<Year>(std::stoi(date.substr(0, 4)));
            Month month = static_cast<Month>(std::stoi(date.substr(5, 2))); // static_cast needed to convert int to Month (enum)
            Day day = static_cast<Day>(std::stoi(date.substr(8, 2)));
            return Date(day, month, year);
        }

        inline void throw_error(const std::string& msg){
            throw std::invalid_argument("CUSIP: " + _cusip + " " + msg);
        }

        void gen_couponflow(){
            // handle logic for generating coupon flow
            if (_couponflow_dirty){
                for (auto& it: _bond -> cashflows()){
                    _couponflow_dates.push_back(it -> date());
                    _couponflow_amounts.push_back(it -> amount());
                }
                if (_couponflow_dates.empty()){
                    throw_error("Bond must have at least 1 cashflow, including par payment.");
                }
                if (fabs(_couponflow_amounts.back() - _price_maturity) > 0.0001){
                    throw_error("Bond lack par payment.");
                }
                int m = _couponflow_dates.size();
                if ((m >= 2) && (_couponflow_dates[m - 2] == _couponflow_dates[m - 1])){
                    throw_error("Last coupon date not on maturity date.");
                }
                _couponflow_dates.pop_back();
                _couponflow_amounts.pop_back();
                _couponflow_dirty = false;
            }
        }

        float get_yield(float price_begin, Date date_begin){
            if(!_use_adjusted_ytm){
                return static_cast<float>(_bond -> yield(price_begin, _day_count, Compounded, _frequency, date_begin));
            } else{
                gen_couponflow();
                std::vector<float> couponflow_amounts_after;
                // create the flow of coupon amounts strictly after the date_begin
                for (int i = 0; i < (int)_couponflow_dates.size(); ++i){
                    if (_couponflow_dates[i] > date_begin){
                        float c = _couponflow_amounts[i];
                        if ((i == 0) && (_couponflow_dates[i - 1] <= date_begin)){
                            c -= static_cast<float>(_bond -> accruedAmount(date_begin));
                        }
                        couponflow_amounts_after.push_back(c);
                    }
                }
                if (couponflow_amounts_after.empty()){
                    throw_error("No coupon found after date_begin in function get_yield().");
                }
                float lb = -1.0;
                float ub = 1.0;
                int MAX_TRIES = 50;
                ++MAX_TRIES;
                float ytm = 0.0;
                while (--MAX_TRIES){
                    ytm = (lb + ub) / 2.0;
                    float p = -price_begin;
                    float d = 1.0;
                    for (float c: couponflow_amounts_after){
                        // TODO: fix case for zeros
                        d *= 1 + ytm * c / (_coupon_rate * _price_maturity);
                        p += c / d;
                    }
                    p += _price_maturity / d;
                    if (fabs(p) < 0.00001 * price_begin){
                        break;
                    } else if (p > 0){
                        lb = ytm;
                    } else{
                        ub = ytm;
                    }
                }
                if (MAX_TRIES == 0){
                    throw_error("YTM search failed. Please check input.");
                }
                return ytm;
            }
        }

        float get_yield_from_issue(){
            if (_yield_from_issue_dirty){
                _yield_from_issue = get_yield(_price_issue, _date_start);
                _yield_from_issue_dirty = false;
            }
            return _yield_from_issue;
        }

        float get_yield_from_purchase(){
            if (_yield_from_purchase_dirty){
                _yield_from_purchase = get_yield(_price_purchase, _date_purchase);
                _yield_from_purchase_dirty = false;
            }
            return _yield_from_purchase;
        }

        inline int get_years_left(Date date){
            // get number of full years left. TODO:  definition of full year is vague.
            return (int)(static_cast<int>(_date_maturity - date) / 365.25);
        }

        inline float get_OID_deminimis(){
            return 0.25 * get_years_left(_date_start);
        }

        inline float get_MD_deminimis(){
            return 0.25 * get_years_left(_date_purchase);
        }

        float get_clean_price_quantlib(float yield, Date date){
            if (date >= _date_maturity){
                return _price_maturity;
            }
            return _bond -> cleanPrice(
                    static_cast<Rate>(yield), 
                    _day_count, 
                    Compounded, 
                    _frequency, 
                    date
                );
        }