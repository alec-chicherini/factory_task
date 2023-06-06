#include <iostream> ///cout
#include <syncstream> 
#include <queue>
#include <random>
#include <sstream>
#include <memory>
#include <cassert>
#include <iomanip> ///std::setprecision
#include <mutex>

using std::cout;
using std::endl;

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

struct Truck {
	Truck() {
		type = static_cast<TRUCK_TYPE>(2 << GetRandomNumber(0, 3));
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
	double capacity_limit_;
	double capacity_now_{ 0.0 };
	std::queue<ConcreteItemPtr> queue_;
	std::vector<Shop> shops_;
	std::mutex mtx_capacity;

	Storage(double capacity_limit):capacity_limit_(capacity_limit) {
		size_t number_of_shops = static_cast<size_t>(GetRandomNumber(5, 10));
		shops_ = std::vector<Shop>(number_of_shops);
	};

	void StoreItem(ConcreteItemPtr concrete_item) {
		queue_.push(concrete_item);
		capacity_now_ += concrete_item->item_.weight;

		while(capacity_now_ > capacity_limit_){
			Truck truck;
			while (true) {
				if (queue_.size() == 0) {
					break;
				}
				auto& item_to_send = queue_.front();
				double truck_capacity_new = truck.capacity_now_ + item_to_send->item_.weight;
				if ( CapacityFromTruck(truck) > truck_capacity_new ) {
					capacity_now_ -= item_to_send->item_.weight;
					truck.cargo.push_back(item_to_send);
					truck.capacity_now_ = truck_capacity_new;
					queue_.pop();
				}
				else {
					size_t random_shop_to_send = static_cast<size_t>(GetRandomNumber(0, (int)shops_.size() - 1));
					std::cout << "{ " << truck << ", \"items\": [";
					for (int i = 0; i < truck.cargo.size(); i++) {
						std::cout << "\"" << truck.cargo[i]->uid_ << "\"";
						if (i != truck.cargo.size() - 1)std::cout << ",";
						shops_.at(random_shop_to_send).store.push_back(truck.cargo[i]);
					}
					cout << " ] }"<< endl;
					break;
				}
			}
		}
	};
};

int main() {
	///INIT BEGIN
	std::vector<FactoryPtr> factories;
	int number_of_factories = GetRandomNumber(3, 10);
	double rate_base = static_cast<double>(GetRandomNumber(50, 100));
	double capacity_production_overall{ 0.0 };
	while (number_of_factories--) {
		factories.push_back(std::make_shared<Factory>(Item(), rate_base));
		if ((std::numeric_limits<double>::max() - factories.back()->rate_) < capacity_production_overall) {
			cout << "ERROR:Factories have too much production. exit."<<endl;
			exit(EXIT_FAILURE);
		}
		capacity_production_overall += factories.back()->rate_;

	}
	cout << "Created " << factories.size() << " factories with rate ["
		      << factories.front()->rate_ << "," << factories.back()->rate_ << "]" << endl;

	double storage_Multiplier = static_cast<double>(GetRandomNumber(100, 1'000));
	if ((std::numeric_limits<double>::max()/ capacity_production_overall) < storage_Multiplier) {
		cout << "ERROR:Storage cant store too much production.exit." << endl;
		exit(EXIT_FAILURE);
	}
	Storage store(storage_Multiplier * capacity_production_overall * 0.95);
	cout << "Created storage with " << store.shops_.size() << " shops. Store capacity limit[95%] = "
		 << std::setprecision(10) <<store.capacity_limit_ << endl;

	int hours_of_working = GetRandomNumber(100, 1000);
	cout << "Simulation started for " << hours_of_working << " hours" << endl;
	///INIT END

	///WORK BEGIN
	for(int i=1;i<=hours_of_working;i++) {
		for (auto& f : factories) {
			auto item = f->produce();
			cout << "{ \"hour\": " << i 
				 << ", " << f 
				 << ", " << item <<" }"<< endl;
			store.StoreItem(item);
		}
	}
	///WORK END
};
