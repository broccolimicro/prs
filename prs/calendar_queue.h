#pragma once

#include <deque>
#include <vector>
#include <stdint.h>
#include <limits>

#include <stdio.h>

// This version slows down over time. Everything is implemented to be O(1) or
// amortized O(1) time, and it's not slowing down in relation to the number of
// elements, but just over usage time. My current guess is that in the
// beginning, the backing container (deque) is first allocated and elements are
// taken in order to place in the calendar. However, as elements are added and
// removed, those memory locations become shuffled. This causes more and more
// cache misses over time, slowing down the push and pop latencies.

// The other guess is that maybe my list merge, split, remove, and insert
// operators are buggy and resulting in lists that are out of order. As a
// result of this, more and more nodes are out of order in their days resulting
// in more and more lookups that have to scan the entire queue. I'll be testing
// for this first.

template <typename T>
struct default_priority {
	uint64_t operator()(const T &value) {
		return (uint64_t)value;
	}
};

template <typename T, typename P=default_priority<T> >
struct calendar_queue {
	struct event {
		event(size_t index) {
			next = nullptr;
			prev = nullptr;
			this->index = index;
		}

		~event() {
		}

		T value;
		size_t index;

		event *next;
		event *prev;
	};

	P priority;

	uint64_t count;
	uint64_t now;

	std::deque<event> events;
	event *unused;

	std::vector<std::pair<event*, event*> > calendar;

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
		this->unused = nullptr;
		calendar.resize(days(), std::pair<event*, event*>(nullptr, nullptr));
	}

	calendar_queue(const calendar_queue &q) {
		count = q.count;
		now = q.now;
		events = q.events;
		for (auto e = events.begin(); e != events.end(); e++) {
			if (e->next != nullptr) {
				e->next = &events[e->next->index];
			}
			if (e->prev != nullptr) {
				e->prev = &events[e->prev->index];
			}
		}
		unused = nullptr;
		if (q.unused != nullptr) {
			unused = &events[q.unused->index];
			unused->prev = nullptr;
		}
		event u = unused;
		for (event *e = q.unused->next; e != nullptr; e = e->next) {
			u->next = &events[e->index];
			u->next->prev = u;
			u = u->next;
		}
		u->next = nullptr;

		calendar = q.calendar;
		for (auto d = calendar.begin(); d != calendar.end(); d++) {
			if (d->first != nullptr) {
				d->first = &events[d->first->index];
			}
			if (d->second != nullptr) {
				d->second = &events[d->second->index];
			}
		}

		/*devs.reserve(q.devs.size());
		for (int i = 0; i < (int)q.devs.size(); i++) {
			devs.push_back(q.devs[i] == nullptr ? nullptr : &events[q.devs[i]->index]);
		}*/

		year = q.year;
		day = q.day;
		mindiff = q.mindiff;
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
			/*uint64_t f0, f1, f2, r0, r1, r2;
			f0 = flength(i);
			r0 = rlength(i);
			f1 = flength(i+1);
			r1 = rlength(i+1);*/

			// merge calendar[i] and calendar[i+1]
			if (calendar[i].second == nullptr) {
				calendar[i] = calendar[i+1];
			} else if (calendar[i+1].first != nullptr) {
				event *e0 = calendar[i].second;
				event *e1 = calendar[i+1].second;
				uint64_t y0 = yearof(priority(e0->value));
				uint64_t y1 = yearof(priority(e1->value));
				uint64_t sy0 = yearof(priority(calendar[i].first->value));
				uint64_t sy1 = yearof(priority(calendar[i+1].first->value));
				while (e1 != nullptr) {
					if (y0 <= y1) {
						event *s1 = nullptr;
						if (y1 != sy1 and e0 != nullptr) {
							// if most events are in the same year, then we don't need to do
							// this search most of the time.
							// if e0 is nullptr, then we can move the entire list over
							for (s1 = e1->prev; s1 != nullptr and yearof(priority(s1->value)) == y1; s1 = s1->prev);
						}
						// by definition, e1 will not be nullptr because there would be
						// nothing to move over
						
						if (s1 == nullptr) {
							calendar[i+1].first->prev = e0;
						} /*else if (s1->next == nullptr) {
							// this shouldn't happen by definition of s1
						}*/ else if (s1->next != nullptr) {
							s1->next->prev = e0;
						}

						event *n = calendar[i].first;
						if (e0 == nullptr) {
							calendar[i].first->prev = e1;
						} else if (e0->next == nullptr) {
							calendar[i].second = e1;
						} else {
							e0->next->prev = e1;
						}

						/*if (e1->next == nullptr) {
							// already end of list, nothing needs to happen here
						} else */ if (e1->next != nullptr) {
							e1->next->prev = nullptr;
						}

						if (e0 == nullptr) {
							// this is broken
							e1->next = n;
							calendar[i].first = (s1 == nullptr ? calendar[i+1].first : s1->next);
						} else {
							e1->next = e0->next;
							e0->next = (s1 == nullptr ? calendar[i+1].first : s1->next);
						}

						if (s1 != nullptr) {
							s1->next = nullptr;
						}

						e1 = s1;
						if (e1 != nullptr) {
							y1 = yearof(priority(e1->value));
						}
					} else if (y0 != sy0) {
						for (e0 = e0->prev; e0 != nullptr and yearof(priority(e0->value)) == y0; e0 = e0->prev);
						y0 = yearof(priority(e0->value));
					} else {
						e0 = nullptr;
					}
				}
			}
			calendar[i+1].first = nullptr;
			calendar[i+1].second = nullptr;

			if (i != 0) {
				calendar[i/2] = calendar[i];
			}

			/*f2 = flength(i/2);
			r2 = rlength(i/2);
			if (f0 != r0 or f1 != r1 or f2 != r2 or f0+f1 != f2 or r0+r1 != r2) {
				printf("shrink f0:%lu r0:%lu f1:%lu r1:%lu f2:%lu r2:%lu\n", f0, r0, f1, r1, f2, r2);
			}*/
		}
		day++;
		calendar.erase(calendar.begin()+days(), calendar.end());
		printf("shrinking %lu\n", days());
	}

	void grow() {
		day--;
		printf("growing %lu\n", days());
		calendar.resize(days(), std::pair<event*, event*>(nullptr, nullptr));
		for (int i = (int)calendar.size()-1; i >= 0; i--) {

			/*uint64_t f0, f1, f2, r0, r1, r2;
			f0 = flength(i);
			r0 = rlength(i);*/

			if (calendar[i].first == nullptr) {
				continue;
			}
			if (i != 0) {
				calendar[i*2] = calendar[i];
			}

			event *e = calendar[i*2].second;
			uint64_t y0 = yearof(priority(calendar[i*2].first->value));
			uint64_t t = priority(e->value);
			uint64_t y = yearof(t);
			uint64_t d = dayof(t);
			while (e != nullptr and (y > y0 or d != i*2)) {
				while (e != nullptr and d == i*2) {
					e = e->prev;
					if (e != nullptr) {
						t = priority(e->value);
						y = yearof(t);
						d = dayof(t);
						if (y == y0 and d == i*2) {
							e = nullptr;
						}
					}
				}
				if (e == nullptr) {
					break;
				}

				event *s = e;
				while (s != nullptr and dayof(priority(s->value)) == d) {
					s = s->prev;
				}

				if (e->next == nullptr) {
					calendar[i*2].second = s;
				} else {
					e->next->prev = s;
				}
				/*if (s == nullptr) {
					// Then calendar[i*2].first->prev is already nullptr
				} else*/ if (s != nullptr) {
					s->next->prev = nullptr;
				}

				event *n = e->next;
				e->next = calendar[i*2+1].first;
				
				if (s == nullptr) {
					calendar[i*2+1].first = calendar[i*2].first;
				} else {
					calendar[i*2+1].first = s->next;
				}

				if (e->next == nullptr) {
					calendar[i*2+1].second = e;
				} else {
					e->next->prev = e;
				}

				if (s == nullptr) {
					calendar[i*2].first = n;
				} else {
					s->next = n;
				}

				e = s;
				if (e != nullptr) {
					t = priority(e->value);
					y = yearof(t);
					d = dayof(t);
				}
			}

			if (i != 0) {
				calendar[i].first = nullptr;
				calendar[i].second = nullptr;
			}

			/*f1 = flength(i*2);
			r1 = rlength(i*2);
			f2 = flength(i*2+1);
			r2 = rlength(i*2+1);
			if (f0 != r0 or f1 != r1 or f2 != r2 or f1+f2 != f0 or r1+r2 != r0) {
				printf("grow f0:%lu r0:%lu f1:%lu r1:%lu f2:%lu r2:%lu\n", f0, r0, f1, r1, f2, r2);
			}*/
		}
	}

	event *find(uint64_t day, uint64_t time) {
		event *e;
		//printf("start find\n");
		for (e = calendar[day].first; e != nullptr and priority(e->value) < time; e = e->next) {
			//printf("find %lu < %lu\n", priority(e->value), time);
		}
		/*if (e == nullptr) {
			printf("not found\n");
		} else {
			printf("found %lu\n", priority(e->value));
		}*/
		return e;
	}

	event *next(uint64_t time) {
		if (empty()) {
			return nullptr;
		}

		auto start = calendar.begin()+dayof(time);
		int y = yearof(time);
		event *m = nullptr;
		uint64_t mt;
		//int steps = 0;
		for (auto day = start; day != calendar.end(); day++) {
			event *e = day->first;// = find(d, time);
			while (e != nullptr and priority(e->value) < time) {
				e = e->next;
			}
			//steps+=flength(d);
			if (e != nullptr) {
				uint64_t et = priority(e->value);
				if (yearof(et) == y) {
					/*if (steps > 10) {
						printf("next one:%d\n", steps);
					}*/
					return e;
				}

				if (m == nullptr or et < mt) {
					m = e;
					mt = et;
				}
			}
		}
		y++;

		for (auto day = calendar.begin(); day != start; day++) {
			//event *e = find(d, time);
			event *e = day->first;// = find(d, time);
			while (e != nullptr and priority(e->value) < time) {
				e = e->next;
			}

			//steps+=flength(d);
			if (e != nullptr) {
				uint64_t et = priority(e->value);
				if (yearof(et) == y) {
					/*if (steps > 10) {
						printf("next two:%d\n", steps);
					}*/
					return e;
				}
				if (m == nullptr or et < mt) {
					m = e;
					mt = et;
				}
			}
		}
		
		/*if (steps > 10) {
			printf("next three:%d\n", steps);
		}*/
		return m;
	}

	void add(event *e) {
		uint64_t t = priority(e->value);
		uint64_t d = dayof(t);

		/*uint64_t f0, f1, r0, r1;
		f0 = flength(d);
		r0 = rlength(d);*/

		//event *n = find(d, t);
		event *n = calendar[d].first;
		while (n != nullptr and priority(n->value) < t) {
			n = n->next;
		}

		if (n == nullptr) {
			if (calendar[d].second == nullptr) {
				calendar[d].first = e;
				calendar[d].second = e;
			} else {
				calendar[d].second->next = e;
				e->prev = calendar[d].second;
				calendar[d].second = e;
			}
		} else {
			e->prev = n->prev;
			e->next = n;
			if (n->prev == nullptr) {
				calendar[d].first = e;
			} else {
				n->prev->next = e;
			}
			n->prev = e;
		}
		if (t < now) {
			now = t;
		}
		count++;
		
		/*f1 = flength(d);
		r1 = rlength(d);
		if (f0 != r0 or f1 != r1 or f1 != f0+1 or r1 != r0+1) {
			printf("add f0:%lu r0:%lu f1:%lu r1:%lu\n", f0, r0, f1, r1);
		}*/

	}

	event *rem(event *e) {
		if (e == nullptr) {
			return nullptr;
		}

		uint64_t d = dayof(priority(e->value));
		/*uint64_t f0, f1, r0, r1;
		f0 = flength(d);
		r0 = rlength(d);*/
		if (e->prev == nullptr) {
			calendar[d].first = e->next;
		} else {
			e->prev->next = e->next;
		}

		if (e->next == nullptr) {
			calendar[d].second = e->prev;
		} else {
			e->next->prev = e->prev;
		}
		e->next = nullptr;
		e->prev = nullptr;
		count--;
		/*f1 = flength(d);
		r1 = rlength(d);
		if (f0 != r0 or f1 != r1 or f1 != f0-1 or r1 != r0-1) {
			printf("rem f0:%lu r0:%lu f1:%lu r1:%lu\n", f0, r0, f1, r1);
		}*/
		return e;
	}

	void set(event *e, T value) {
		if (priority(value) < priority(e->value)) {
			e->value = value;
			add(rem(e));
		}
	}

	event *push(T value) {
		event *result = nullptr;
		/*if (value.dev < devs.size() and devs[value.dev] != nullptr) {
			// event already exists
			result = devs[value.dev];
			set(result, value);
			return result;
		} else {*/
			if (unused != nullptr) {
				result = unused;
				unused = unused->next;
				if (unused != nullptr) {
					unused->prev = nullptr;
				}
				result->next = nullptr;
			} else {
				events.push_back(event(events.size()));
				result = &events.back();
			}
			/*if (value.dev >= devs.size()) {
				devs.resize(value.dev+1, nullptr);
			}
			devs[value.dev] = result;
		}*/

		result->value = value;
		add(result);
		if (day > 0 and count > (days()<<1)) {
			grow();
		}
		return result;
	}

	T pop(event *e) {
		e = rem(e);
		if (e == nullptr) {
			return T();
		}
		e->next = unused;
		e->prev = nullptr;
		if (unused != nullptr) {
			unused->prev = e;
		}
		unused = e;
		if (year-day > mindiff and count < (days()>>1)) {
			shrink();
		}
		return e->value;
	}

	T pop(uint64_t time=std::numeric_limits<uint64_t>::max()) {
		if (time == std::numeric_limits<uint64_t>::max()) {
			time = now;
		}
		event *e = next(time);
		if (time == now and e != nullptr) {
			now = priority(e->value);
		}
		return pop(e);
	}

	/*bool has(int dev) {
		return dev < (int)devs.size() and devs[dev] != nullptr;
	}*/

	uint64_t size() {
		return count;
	}

	uint64_t flength(int d) {
		uint64_t result = 0;
		for (event *e = calendar[d].first; e != nullptr; e = e->next) {
			result++;
		}
		return result;
	}

	uint64_t rlength(int d) {
		uint64_t result = 0;
		for (event *e = calendar[d].second; e != nullptr; e = e->prev) {
			result++;
		}
		return result;
	}

	bool empty() {
		return count == 0;
	}
};

