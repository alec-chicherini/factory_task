#include <iostream> ///cout
#include <syncstream> 
#include <queue>
#include <random>
#include <sstream>
#include <memory>
#include <cassert>
#include <iomanip> ///std::setprecision
#include <mutex>
#include <thread>
#include <map>

using std::cout;
using std::endl;
using std::osyncstream;

int GetRandomNumber(int from, int to) {
	std::random_device random_device_;
	std::uniform_int_distribution<std::mt19937::result_type> dist(from, to);
	return dist(random_device_);
}

enum class PACKAGE_TYPE{
	TYPE_1=0,
	TYPE_2,
	TYPE_3,
	TYPE_4
};

struct Item {
	std::string name;
	PACKAGE_TYPE type;
	double weight;
	static int id;
	Item() {
		type = static_cast<PACKAGE_TYPE>(GetRandomNumber(0, 3));
		weight = static_cast<double>(GetRandomNumber(1, 100));
		id = id + 1;
		name = "product_";
		name.append(std::to_string(id));

	};
};
int Item::id {0};


struct ConcreteItem {
	ConcreteItem(Item item) :item_(item) {
			std::stringstream ss;
			const int size_of_uid = 8;
			while (ss.str().size() <= size_of_uid) {
				ss << std::hex << GetRandomNumber(0,15);
			}
			uid_=ss.str();
	};

	std::string uid_;
	Item item_;
};
using ConcreteItemPtr = std::shared_ptr<ConcreteItem>;
std::ostream& operator<<(std::ostream& os, ConcreteItemPtr const& c_i)
{
	return os << "\"name_product\": \"" << c_i->item_.name 
		      << "\", \"uid\": \"" << c_i->uid_ << "\"";
}

struct Factory {
	Factory(Item item, double rate_base) :item_(item) {
		rate_ = rate_base + rate_addition;
		rate_addition = rate_addition + 0.1;
		id = id + 1;
		name = "factory_";
		name.append(std::to_string(id));
	};

	ConcreteItemPtr produce() {
		return std::make_shared<ConcreteItem>(item_);
	};

	Item item_;
	double rate_;
	static double rate_addition;
	std::string name;
	static int id;
};
using FactoryPtr = std::shared_ptr<Factory>;
int Factory::id {0};
double Factory::rate_addition {0.0};
std::ostream& operator<<(std::ostream& os, FactoryPtr const& f)
{
	return os << "\"name_factory\": \"" << f->name
		      << "\", \"count\": \"" << std::setprecision(9) << f->rate_ << "\"";
}

enum class TRUCK_TYPE {
	TONN_2 = 2,
	TONN_4 = 4,
	TONN_8 = 8,
	TONN_16 = 16
};
using statistics = std::map<PACKAGE_TYPE, double>;
statistics stat_global{ {PACKAGE_TYPE::TYPE_1,0.0},{PACKAGE_TYPE::TYPE_2,0.0},{PACKAGE_TYPE::TYPE_3,0.0},{PACKAGE_TYPE::TYPE_4,0.0} };

std::mutex mtx_statistics;
struct Truck {
	Truck() {
		type = static_cast<TRUCK_TYPE>(2 << GetRandomNumber(0, 3));
	};
	~Truck() {
		statistics stat_this_truck{ {PACKAGE_TYPE::TYPE_1,0.0},{PACKAGE_TYPE::TYPE_2,0.0},{PACKAGE_TYPE::TYPE_3,0.0},{PACKAGE_TYPE::TYPE_4,0.0} };
		for (auto& i : cargo) {
			stat_this_truck[i->item_.type] += i->item_.weight;
		}
		mtx_statistics.lock();
		for (auto& s : stat_global) {
			s.second = (s.second + stat_this_truck[s.first]) / 2.0;
		}
		mtx_statistics.unlock();
	};
	TRUCK_TYPE type;
	std::vector<ConcreteItemPtr> cargo;
	double capacity_now_{ 0.0 };
};

std::ostream& operator<<(std::ostream& os, Truck const& t)
{
	return os << "\"truck_tonn\": " << static_cast<int>(t.type)
		      << ", \"truck_capacity\": " << std::setprecision(9)<< t.capacity_now_;
}

double CapacityFromTruck(Truck truck)noexcept {
	return static_cast<double>(truck.type) * 1000.0;
};

struct Shop {
	std::vector<ConcreteItemPtr> store;
};

struct Storage {
	Storage(double capacity_limit):capacity_limit_(capacity_limit) {
		size_t number_of_shops = static_cast<size_t>(GetRandomNumber(5, 10));
		shops_ = std::vector<Shop>(number_of_shops);
	};

	void StoreItem(ConcreteItemPtr concrete_item) {
		QueuePush(concrete_item);
		PlusCapacity(concrete_item->item_.weight);

		while (GetCapacity() > capacity_limit_) {
			auto send_items = [this] {

				while (true) {
					std::lock_guard lock(mtx_queue);
					if (QueueSize() == 0) {
						break;
					}
					Truck truck;
					auto item_to_send = QueueFront();
					double truck_capacity_new = truck.capacity_now_ + item_to_send->item_.weight;
					if (CapacityFromTruck(truck) > truck_capacity_new) {
						MinusCapacity(item_to_send->item_.weight);
						truck.cargo.push_back(item_to_send);
						truck.capacity_now_ = truck_capacity_new;
						QueuePop();
					}
					else {
						size_t random_shop_to_send = static_cast<size_t>(GetRandomNumber(0, (int)shops_.size() - 1));
						std::stringstream ss;
						ss << "{ " << truck << ", \"items\": [";
						for (int i = 0; i < truck.cargo.size(); i++) {
							ss << "\"" << truck.cargo[i]->uid_ << "\"";
							if (i != truck.cargo.size() - 1)ss << ",";
							shops_.at(random_shop_to_send).store.push_back(truck.cargo[i]);
						}
						ss << " ] }" << endl;

						osyncstream(cout) << ss.str();
						break;
					}
				}
			};

			std::thread thread_sender(send_items);
			thread_sender.detach();
		}
	};
	size_t GetShopsSize() {
		return shops_.size();
	}
	double GetCapacityLimit() {
		return capacity_limit_;
	}

private:
	double capacity_limit_;
	double capacity_now_{ 0.0 };
	std::queue<ConcreteItemPtr> queue_;
	std::vector<Shop> shops_;
	std::mutex mtx_capacity;
	std::mutex mtx_queue;

	void PlusCapacity(double value) {
		std::lock_guard lock(mtx_capacity);
		capacity_now_ = capacity_now_ + value;
	};
	void MinusCapacity(double value) {
		std::lock_guard lock(mtx_capacity);
		capacity_now_ = capacity_now_ - value;
	};
	double GetCapacity() {
		std::lock_guard lock (mtx_capacity);
		return capacity_now_;
	};
	void QueuePop() {
		queue_.pop();
	}

	void QueuePush(ConcreteItemPtr concrete_item) {
		std::lock_guard lock(mtx_queue);
		queue_.push(concrete_item);
	}

	size_t QueueSize() {
		return queue_.size();
	}

	ConcreteItemPtr QueueFront() {
		return queue_.front();
	}

};

int main() {
	///CONFIG BEGIN
	constexpr int MAX_DETACHED_THREADS = 16;
	constexpr int MAX_NUMBER_OF_FACTORIES = 10;
	constexpr int MAX_RATE_BASE = 100;
	constexpr int MAX_STORAGE_MULTIPIYER = 150;
	constexpr int MIN_HOURS_OF_WORKING = 100;
	constexpr int MAX_HOURS_OF_WORKING = 10000;
	///CONFIG END

	///INIT BEGIN
	std::vector<FactoryPtr> factories;
	int number_of_factories = GetRandomNumber(3, MAX_NUMBER_OF_FACTORIES);
	double rate_base = static_cast<double>(GetRandomNumber(50, MAX_RATE_BASE));
	double capacity_production_overall{ 0.0 };
	while (number_of_factories--) {
		factories.push_back(std::make_shared<Factory>(Item(), rate_base));
		if ((std::numeric_limits<double>::max() - factories.back()->rate_) < capacity_production_overall) {
			std::cerr << "ERROR:Factories have too much production. exit."<<endl;
			exit(EXIT_FAILURE);
		}
		capacity_production_overall += factories.back()->rate_;

	}
	cout << "Created " << factories.size() << " factories with rate ["
		      << factories.front()->rate_ << "," << factories.back()->rate_ << "]" << endl;

	double storage_Multiplier = static_cast<double>(GetRandomNumber(100, MAX_STORAGE_MULTIPIYER));
	if ((std::numeric_limits<double>::max()/ capacity_production_overall) < storage_Multiplier) {
		std::cerr << "ERROR:Storage cant store too much production.exit." << endl;
		exit(EXIT_FAILURE);
	}
	Storage store(storage_Multiplier * capacity_production_overall * 0.95);
	cout << "Created storage with " << store.GetShopsSize() << " shops. Store capacity limit[95%] = "
		 << std::setprecision(10) <<store.GetCapacityLimit() << endl;

	int hours_of_working = GetRandomNumber(MIN_HOURS_OF_WORKING, MAX_HOURS_OF_WORKING);
	cout << "Simulation started for " << hours_of_working << " hours" << endl;
	///INIT END

	///WORK BEGIN
	std::atomic<int> detached_thread_counter;
	constexpr bool WAS_DETACHED = true;
	constexpr bool WAS_JOINED = false;

	for(int i=1;i<=hours_of_working;i++) {
		auto day_work = [i,&factories, &store,&detached_thread_counter](bool action) {
			for (auto& f : factories) {
				auto item = f->produce();
				osyncstream(cout) << "{ \"hour\": " << i
					<< ", " << f
					<< ", " << item << " }" << endl;
				store.StoreItem(item);
			}
			if (action == WAS_DETACHED) {
				--detached_thread_counter;
			}
		};

		if (detached_thread_counter.load() < MAX_DETACHED_THREADS) {
			std::thread thread_worker(day_work, WAS_DETACHED);
			thread_worker.detach();
			++detached_thread_counter;
		}
		else {
			std::thread thread_worker(day_work, WAS_JOINED);
			thread_worker.join();
			
		}
	}

	while (detached_thread_counter.load() != 0) {
		///wait threads
	}
	///WORK END

	///STATISTICS BEGIN
	cout << "Average type of items in truck:" << endl;
	cout << "type - amount:" << endl;
	for (auto stat : stat_global) {
		cout << static_cast<int>(stat.first)<<" - "<<std::setprecision(9)<<stat.second << endl;
	}
	///STATISTICS END
};
