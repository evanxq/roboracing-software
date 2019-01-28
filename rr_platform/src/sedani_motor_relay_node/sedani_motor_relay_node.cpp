#include <ros/ros.h>
#include <rr_platform/speed.h>
#include <rr_platform/steering.h>
#include <rr_platform/chassis_state.h>
#include <SerialPort.h>

double desiredSpeed = 0;
double desiredSteer = 0;
double prevAngle = 0;
double prevSpeed = 0;

ros::Publisher state_pub;

/**
 * @note http://stackoverflow.com/a/27511119
 */
std::vector <std::string> split(const std::string &s, char delim) {
    std::stringstream ss(s);
    std::string item;
    std::vector <std::string> elems;
    while (std::getline(ss, item, delim)) {
        elems.push_back(std::move(item));
    }
    return elems;
}

void SpeedCallback(const rr_platform::speed::ConstPtr &msg) {
    desiredSpeed = msg->speed;
}

void SteeringCallback(const rr_platform::steering::ConstPtr &msg) {
    desiredSteer = msg->angle;
}

void sendCommand(SerialPort &port) {
    std::string message = "$" + std::to_string(desiredSpeed) + ", " + std::to_string(desiredSteer) + "\n";
    port.Write(message);
}

void publishData(const std::string &line) {
    if (line.empty()) {
        return;
    }
    std::vector <std::string> data = split(line.substr(1), ',');
    rr_platform::chassis_state msg;
    msg.header.stamp = ros::Time::now();
    msg.speed_mps = std::stod(data[1]);
    msg.steer_rad = std::stod(data[2]);
    msg.state = data[0];
    msg.mux_automatic = (data[0] == "1.00" || data[0] == "3.00");
    msg.estop_on = (data[0] == "2.00");
    state_pub.publish(msg);
}

int main(int argc, char **argv) {
    ros::init(argc, argv, "sedani_motor_relay_node");
    ros::NodeHandle nh;
    ros::NodeHandle nhp("~");

    // Subscribers
    std::string speed_topic_name;
    nhp.param(std::string("speed_topic"), speed_topic_name, std::string("/speed"));
    ros::Subscriber speed_sub = nh.subscribe(speed_topic_name, 1, SpeedCallback);

    std::string steering_topic_name;
    nhp.param(std::string("steering_topic"), steering_topic_name, std::string("/steering"));
    ros::Subscriber steering_sub = nh.subscribe(steering_topic_name, 1, SteeringCallback);

    state_pub = nh.advertise<rr_platform::chassis_state>("/chassis_state", 1);

    ROS_INFO_STREAM("Listening for speed on " << speed_topic_name);
    ROS_INFO_STREAM("Listening for steer on " << steering_topic_name);

    // Serial port setup
    std::string serial_port_name;
    nhp.param(std::string("serial_port"), serial_port_name, std::string("/dev/ttyACM0"));

    SerialPort serial_port;
    if(!serial_port.Open(serial_port_name, 115200)) {
        ROS_FATAL_STREAM("Unable to open serial port: " << serial_port_name);
        return 1;
    }

    ROS_INFO("Sedani motor relay node is ready.");

    float hz = 20;
    ros::Rate rate(hz);

    ros::Duration(2.0).sleep();

    while (ros::ok()) {
        ros::spinOnce();
        if (desiredSteer != prevAngle || desiredSpeed != prevSpeed) {
            ROS_INFO("Sending command: servo=%f, motor=%f", desiredSteer, desiredSpeed);
        }

        prevAngle = desiredSteer;
        prevSpeed = desiredSpeed;

        sendCommand(serial_port);

        auto ret = serial_port.ReadLine();
        ROS_INFO_STREAM("motor relay received " << ret);
        publishData(serial_port.ReadLine());
        rate.sleep();
    }

    ROS_INFO("Shutting down Sedani motor relay node.");
    return 0;
}