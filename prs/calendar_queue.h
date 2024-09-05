#pragma once

#include <vector>
#include <stdint.h>
#include <limits>

#include <stdio.h>
#include <algorithm>

// This version slows down over time. Everything is implemented to be O(1) or
// amortized O(1) time, and it's not slowing down in relation to the number of
// elements, but just over usage time. My current guess is that in the
// beginning, the backing container (deque) is first allocated and elements are
// taken in order to place in the calendar. However, as elements are added and
// removed, those memory locations become shuffled. This causes more and more
// cache misses over time, slowing down the push and pop latencies.

template <typename T>
struct default_priority {
	uint64_t operator()(const T &value) {
		return (uint64_t)value;
	}
};

template <typename T, typename P=default_priority<T> >
struct calendar_queue {
	P priority;

	uint64_t count;
	uint64_t now;

	std::vector<std::vector<T> > calendar;

	// bit shift amounts
	int year;
	int day;
	int mindiff;

	calendar_queue(int year=14, int mindiff=4, P priority=P()) {
		this->count = 0;
		this->now = std::numeric_limits<uint64_t>::max();
		this->mindiff = mindiff;
		this->year = year;
		this->day = year < mindiff ? 0 : year-mindiff;
		this->priority = priority;
		calendar.resize(days());
	}

	~calendar_queue() {
	}

	uint64_t timeof(uint64_t day) {
		return day<<this->day;
	}

	uint64_t yearof(uint64_t time) {
		return time>>year;
	}

	uint64_t dayof(uint64_t time) {
		return (time>>day)&((1ul<<(year-day))-1ul);
	}

	uint64_t days() {
		return (1ul<<(year-day));
	}

	void shrink() {
		for (int i = 0; i < (int)calendar.size(); i+=2) {
			/*uint64_t f0, f1, f2;
			f0 = calendar[i].size();
			f1 = calendar[i+1].size();
			if (not is_sorted(calendar[i].begin(), calendar[i].end())) {
				printf("shrink fo0\n");
			}
			if (not is_sorted(calendar[i+1].begin(), calendar[i+1].end())) {
				printf("shrink fo1\n");
			}*/

			// merge calendar[i] and calendar[i+1]
			if (calendar[i].empty()) {
				calendar[i] = calendar[i+1];
			} else if (not calendar[i+1].empty()) {
				auto e0 = calendar[i].rbegin();
				auto e1 = calendar[i+1].rbegin();
				uint64_t y0 = yearof(priority(*e0));
				uint64_t y1 = yearof(priority(*e1));
				uint64_t sy0 = yearof(priority(calendar[i][0]));
				uint64_t sy1 = yearof(priority(calendar[i+1][0]));
				while (e1 != calendar[i+1].rend()) {
					if (y0 <= y1) {
						auto s1 = calendar[i+1].rend();
						if (y1 != sy1 and e0 != calendar[i].rend()) {
							// if most events are in the same year, then we don't need to do
							// this search most of the time.
							// if e0 is nullptr, then we can move the entire list over
							for (s1 = e1+1; s1 != calendar[i+1].rend() and yearof(priority(*s1)) == y1; s1++);
						}
						// by definition, e1 will not be nullptr because there would be
						// nothing to move over
						
						calendar[i].insert(e0.base(), s1.base(), e1.base());

						e1 = s1;
						if (e1 != calendar[i+1].rend()) {
							y1 = yearof(priority(*e1));
						}
					} else if (y0 != sy0) {
						for (e0++; e0 != calendar[i].rend() and yearof(priority(*e0)) == y0; e0++);
						y0 = yearof(priority(*e0));
					} else {
						e0 = calendar[i].rend();
					}
				}
			}
			calendar[i+1].clear();

			if (i != 0) {
				calendar[i/2] = calendar[i];
			}

			/*f2 = calendar[i/2].size();
			if (f0+f1 != f2) {
				printf("shrink f0:%lu f1:%lu f2:%lu\n", f0, f1, f2);
			}
			if (not is_sorted(calendar[i/2].begin(), calendar[i/2].end())) {
				printf("shrink fo2\n");
			}*/
		}
		day++;
		calendar.erase(calendar.begin()+days(), calendar.end());
		printf("shrinking %lu\n", days());
	}

	void grow() {
		day--;
		printf("growing %lu\n", days());
		calendar.resize(days());
		for (int i = (int)calendar.size()-1; i >= 0; i--) {

			/*uint64_t f0, f1, f2;
			f0 = calendar[i].size();
			if (not is_sorted(calendar[i].begin(), calendar[i].end())) {
				printf("grow fo0\n");
			}*/

			if (calendar[i].empty()) {
				continue;
			}
			if (i != 0) {
				calendar[i*2] = calendar[i];
			}

			auto e = calendar[i*2].rbegin();
			uint64_t y0 = yearof(priority(calendar[i*2][0]));
			uint64_t t = priority(*e);
			uint64_t y = yearof(t);
			uint64_t d = dayof(t);
			while (e != calendar[i*2].rend() and (y > y0 or d != i*2)) {
				while (e != calendar[i*2].rend() and d == i*2) {
					e++;
					if (e != calendar[i*2].rend()) {
						t = priority(*e);
						y = yearof(t);
						d = dayof(t);
						if (y == y0 and d == i*2) {
							e = calendar[i*2].rend();
						}
					}
				}
				if (e == calendar[i*2].rend()) {
					break;
				}

				auto s = e;
				while (s != calendar[i*2].rend() and dayof(priority(*s)) == d) {
					s++;
				}

				calendar[i*2+1].insert(calendar[i*2+1].begin(), s.base(), e.base());
				calendar[i*2].erase(s.base(), e.base());

				e = s;
				if (e != calendar[i*2].rend()) {
					t = priority(*e);
					y = yearof(t);
					d = dayof(t);
				}
			}

			if (i != 0) {
				calendar[i].clear();
			}

			/*f1 = calendar[i*2].size();
			f2 = calendar[i*2+1].size();
			if (f1+f2 != f0) {
				printf("grow f0:%lu f1:%lu f2:%lu\n", f0, f1, f2);
			}
			if (not is_sorted(calendar[i*2].begin(), calendar[i*2].end())) {
				printf("grow fo1\n");
			}
			if (not is_sorted(calendar[i*2+1].begin(), calendar[i*2+1].end())) {
				printf("grow fo2\n");
			}*/
		}
	}

	typename std::vector<T>::iterator find(uint64_t day, uint64_t time) {
		auto e = calendar[day].begin();
		while (e != calendar[day].end() and priority(*e) < time) { e++; }
		return e;
	}

	std::pair<int, typename std::vector<T>::iterator> next(uint64_t time) {
		auto start = calendar.begin()+dayof(time);
		int y = yearof(time);
		auto m = calendar.back().end();
		uint64_t mt = std::numeric_limits<uint64_t>::max();
		auto md = calendar.end();
		for (auto di = start; di != calendar.end(); di++) {
			for (auto e = di->begin(); e != di->end(); e++) {
				uint64_t et = priority(*e);
				if (et >= time) {
					if (yearof(et) == y) {
						return std::pair<int, typename std::vector<T>::iterator>(di-calendar.begin(), e);
					}

					if (et < mt) {
						m = e;
						mt = et;
						md = di;
					}
					break;
				}
			}
		}
		y++;

		for (auto di = calendar.begin(); di != start; di++) {
			for (auto e = di->begin(); e != di->end(); e++) {
				uint64_t et = priority(*e);
				if (yearof(et) == y) {
					return std::pair<int, typename std::vector<T>::iterator>(di-calendar.begin(), e);
				}

				if (et < mt) {
					m = e;
					mt = et;
					md = di;
				}
				break;
			}
		}
		
		return std::pair<int, typename std::vector<T>::iterator>(md-calendar.begin(), m);
	}

	void set(typename std::vector<T>::iterator e, T value) {
		if (priority(value) < priority(*e)) {
			calendar[dayof(*e)].erase(e);
			uint64_t t = priority(value);
			uint64_t d = dayof(t);
			auto n = calendar[d].begin();
			while (n != calendar[d].end() and priority(*n) < t) { n++; }
			calendar[d].insert(n, e);
			if (t < now) {
				now = t;
			}
		}
	}

	std::pair<int, typename std::vector<T>::iterator> push(T value) {
		uint64_t t = priority(value);
		uint64_t d = dayof(t);

		/*uint64_t f0, f1;
		f0 = calendar[d].size();
		if (not is_sorted(calendar[d].begin(), calendar[d].end())) {
			printf("push fo0\n");
		}*/

		auto n = calendar[d].begin();
		while (n != calendar[d].end() and priority(*n) < t) { n++; }

		auto result = calendar[d].insert(n, value);
		if (t < now) {
			now = t;
		}
		count++;
		
		/*f1 = calendar[d].size();
		if (f1 != f0+1) {
			printf("push f0:%lu f1:%lu\n", f0, f1);
		}
		if (not is_sorted(calendar[d].begin(), calendar[d].end())) {
			printf("push fo1\n");
		}*/

		if (day > 0 and count > (days()<<1)) {
			grow();
		}
		return std::pair<int, typename std::vector<T>::iterator>(d, result);
	}

	T pop(uint64_t time=std::numeric_limits<uint64_t>::max()) {
		if (time == std::numeric_limits<uint64_t>::max()) {
			time = now;
		}

		auto e = next(time);

		/*uint64_t f0, f1;
		f0 = calendar[e.first].size();
		if (not is_sorted(calendar[e.first].begin(), calendar[e.first].end())) {
			printf("pop fo0\n");
		}*/

		T value = *e.second;
		if (time == now) {
			now = priority(value);
		}
		calendar[e.first].erase(e.second);
		count--;

		/*f1 = calendar[e.first].size();
		if (f1 != f0-1) {
			printf("pop f0:%lu f1:%lu\n", f0, f1);
		}
		if (not is_sorted(calendar[e.first].begin(), calendar[e.first].end())) {
			printf("pop fo1\n");
		}*/

		if (year-day > mindiff and count < (days()>>1)) {
			shrink();
		}
		return value;
	}

	uint64_t size() {
		return count;
	}

	bool empty() {
		return count == 0;
	}
};

