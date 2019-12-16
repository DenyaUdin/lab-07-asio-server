#include <boost/asio.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/array.hpp>

#include <memory>
#include <thread>
#include <chrono>
#include <mutex>
#include <algorithm>
#include <iostream>
using namespace boost;
using namespace boost::asio;
using namespace std::chrono;
using namespace std;
using ip::tcp;

std::mutex mut; 
struct talk_to_client;
extern std::vector <talk_to_client *> clients; 

struct talk_to_client 
{
	talk_to_client(asio::io_service &ios) {  
		sock_ = new ip::tcp::socket(ios); 
		timeout = false; 
	}
	
	
	ip::tcp::socket & sock() { return *sock_; } 
	bool timed_out() const
	{
		return timeout;
	}
	void stop() 
	{
		
		sock_->shutdown(asio::socket_base::shutdown_send);
		boost::system::error_code err; sock_->close(err);
	}

	void writeToSocket(std::string& buf) {  
		std::size_t total_bytes_written = 0;
		while (total_bytes_written != buf.length()) {
			total_bytes_written += sock_->write_some(
				asio::buffer(buf.c_str() +
					total_bytes_written,
					buf.length() - total_bytes_written));
		}
	}

	bool flagFirst = true; 
	void readToWrite() 
	{
		if (flagFirst) cout << endl << "Client connected!\n";
		boost::array<char, 256> buf; 
		boost::system::error_code error; 
		size_t len = sock_->read_some(boost::asio::buffer(buf), error); 
		buf[len] = 0; 
		bool flagNotClient = false; 
		if (buf.data() == nullptr) flagNotClient = true;
		if (!flagNotClient) if (strcmp(buf.data(), "") == 0) flagNotClient = true; 
		if (flagNotClient)  
		{
			system_clock::time_point end = std::chrono::system_clock::now();   
			if (std::chrono::duration_cast<std::chrono::milliseconds>(end - now).count() >= 5000) { 
				
				stop();
				delete sock_;
				timeout = true; 
			}
			return;
		}
		now = std::chrono::system_clock::now(); 
		cout << "From client: " << buf.data(); 
		string strToClient="";
		if (flagFirst) 
		{
			username_ = buf.data(); 
			strToClient = "login_ok\n";  
			flagFirst = false; 
		}
		else { 
			string fromClient = buf.data(); 
			if (fromClient == "ping\n") strToClient = "ping_ok\n";
			else
				if (fromClient == "clients\n") 
				{
				
				for (auto pos : clients)
					strToClient += pos->username_ + " ";
				strToClient += "\n";

				}
				else strToClient = "Unknown format\n"; 
			
		}
		cout << "To client: " << strToClient; 
		writeToSocket(strToClient);  

	}
	

private:
	
	ip::tcp::socket *sock_; 
	bool timeout; 
	std::string username_; 
	system_clock::time_point now; 
	

};

std::vector <talk_to_client *> clients; 

bool predicatTimeOut(talk_to_client *pCl)
{
	return pCl->timed_out();
}

void accept_thread() { 
	asio::io_service ios;
	ip::tcp::acceptor acceptor(ios, ip::tcp::endpoint(ip::tcp::v4(), 3333));
	while (true) { 
		talk_to_client * client = new talk_to_client(ios); 
		acceptor.accept(client->sock()); 
		std::lock_guard <std::mutex> lock(mut);  
		clients.push_back(client); 
	}
}

void handle_clients_thread() { 
	while (true) 
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
		std::lock_guard <std::mutex> lock(mut); 
		for (auto& client : clients) { 
			client->readToWrite(); 
		}
		auto posDel=std::remove_if(clients.begin(), clients.end(), predicatTimeOut); 
		clients.erase(posDel, clients.end()); 
		
	}
}
int main(int argc, char* argv[])
{

	thread t1(accept_thread);
	thread t2(handle_clients_thread);
	t1.join();
	t2.join();
}
