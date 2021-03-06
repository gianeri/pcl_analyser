#include <ros/ros.h>
#include <ros/timer.h>
#include <math.h>
#include <iostream>
#include <string>

// TF
#include <tf/transform_listener.h>
#include <tf/transform_broadcaster.h>
//Messages
#include <geometry_msgs/Vector3.h>
#include <nav_msgs/Path.h>
#include <nav_msgs/Odometry.h>
#include "donkey_rover/Speed_control.h"

#include <Eigen/Dense> 
using namespace Eigen;
double b = 0.4;
class PathfollowerClass
{	
	public:
	
	PathfollowerClass(ros::NodeHandle& node)
	{
		n_=node;
		SubFromPath_		= n_.subscribe("/Path_pso", 1, &PathfollowerClass::pathCallback,this);
		//SubFromOdom_		= n_.subscribe("/odom",     1, &PathfollowerClass::OdomCallback,this);
		//SubFromControllerSpeed_ = n_.subscribe("/speedfollow",     1, &PathfollowerClass::speedcallback,this);
		
		speed_pub_	  	  = n_.advertise<geometry_msgs::Vector3> ("body_error", 1);
		testodom_pub_		  = n_.advertise<nav_msgs::Odometry> ("test_odom", 1);
		speed_control_pub_	  = n_.advertise<donkey_rover::Speed_control> ("speed_control", 1);
		
		//Initializer
		new_path = false;
		//Vx = 0.0;
		//Vy = 0.0;
		//Vth = 0.0;
		x_base_orig  = 0.0;
		y_base_orig  = 0.0;
		th_base_orig = 0.0;
		th_bias = 0.0;
		sub_goal_x = b;
		sub_goal_y = 0.0;
		
		sub_goal_err = 0.12;
		goal_achieved = false;
		set_th_bias = false;
		
		current_time = ros::Time::now();
		last_time = ros::Time::now();
		
	}
	
	void pathCallback(const nav_msgs::Path::ConstPtr& msg)
	{
		ROS_INFO("OBSTACLE AVOIDANCE ACTIVATED: new path received");
		path = *msg;
		new_path = true;
		set_th_bias = true;
		path_size = path.poses.size();
		path_counter = 0;
		
		x_base_orig  = 0.0;
		y_base_orig  = 0.0;		

		
	}
	/*
	void OdomCallback(const nav_msgs::Odometry::ConstPtr& msg)
	{
		double Vx  = msg->twist.twist.linear.x;
		double Vy  = msg->twist.twist.linear.y;
		double Vth = msg->twist.twist.angular.z;
		
		//asynch
		current_time = ros::Time::now();
		double dt = (current_time - last_time).toSec();
		
		last_time = current_time;
		
		
		nav_msgs::Odometry odom;
    		odom.header.stamp = current_time;
    		odom.header.frame_id = "odom";

    		
    		odom.pose.pose.position.x = x_base_orig;
    		odom.pose.pose.position.y = y_base_orig;
    		odom.pose.pose.position.z = 0.0;
    		
    		geometry_msgs::Quaternion odom_quat = tf::createQuaternionMsgFromYaw(th_base_orig);
    		odom.pose.pose.orientation = odom_quat;

    		//set the velocity
    		odom.child_frame_id = "base_link";
    		odom.twist.twist.linear.x = Vx;
    		odom.twist.twist.linear.y = Vy;
    		odom.twist.twist.angular.z = Vth;
		
		testodom_pub_.publish(odom); 
		
		
	
	}*/
	/*
	void speedcallback(const geometry_msgs::Vector3::ConstPtr& msg)
	{
		//if(msg->z > 5.00) goal_achieved = true;
		//else         goal_achieved = false;
	}*/
	
	void run()
	{
		
		
		geometry_msgs::Vector3 body_error;
		ros::Rate rate(100.0);
		Vector2f body_temp;
		Vector2f IF_temp;
		Matrix2f Rot;
		Rot << 1,0,
		       0,1;
		       
		bool first_loop = true;
		bool  transform_present = false;      
		double curr_x;
		double curr_y;
		double curr_yaw;
		double last_x;
		double last_y;
		double last_yaw;	
		double delta_x   = 0.0;
		double delta_y   = 0.0;
		double delta_yaw = 0.0;
		tf::TransformListener listener;
		donkey_rover::Speed_control speed_control;
		
		while (ros::ok())
		{
			tf::StampedTransform transform_odom_laser;
    		    	try{
      				listener.lookupTransform("/odom", "/base_link", ros::Time(0), transform_odom_laser);
      				transform_present = true;
    		    	}
    			catch (tf::TransformException ex)
    			{
      				ROS_WARN("%s",ex.what());
      				ros::Duration(0.05).sleep();
      				transform_present = false;
    			}
			//Reading x and y
			curr_x =transform_odom_laser.getOrigin().x();
			curr_y =transform_odom_laser.getOrigin().y();
			
			tfScalar roll,pitch,yaw;
			
			tf::Matrix3x3 M(transform_odom_laser.getRotation());
			M.getRPY(roll,pitch,yaw,(unsigned int) 1);
			curr_yaw = (double) yaw;
			
			
			if (set_th_bias && transform_present)
			{
				th_bias = curr_yaw - M_PI/2;
				th_base_orig = 0.0;
				set_th_bias = false;
			}
			
			if (!first_loop)
			{	
				
				delta_x   =  (curr_x - last_x) * cos(th_bias)  +  (curr_y - last_y)* sin(th_bias);
				delta_y   = -(curr_x - last_x) * sin(th_bias)  +  (curr_y - last_y)* cos(th_bias);
				//delta_x   = (curr_x - last_x); 
				//delta_y   = (curr_y - last_y);
				
				delta_yaw = (curr_yaw - last_yaw);
			}
						
			last_x = curr_x;
			last_y = curr_y;
			last_yaw = curr_yaw;
			
			if (first_loop && transform_present) 
			{
				first_loop = false;
			
				//ROS_ERROR("initial yaw: %f ",curr_yaw);
				//ROS_INFO("start moving pc!....");	
			}
			
			th_base_orig += delta_yaw;
			x_base_orig += delta_y;
			y_base_orig += -delta_x;
			
			
			//Rot Matrix
			
			
			Rot(0,0) =  cos(th_base_orig);		Rot(0,1) =  sin(th_base_orig);
			
			Rot(1,0) = -sin(th_base_orig);		Rot(1,1) =  cos(th_base_orig);
			
			
			
			
			double dx,dy;
			
			IF_temp(0) = sub_goal_x - x_base_orig;
			IF_temp(1) = sub_goal_y - y_base_orig;
				
			//Apply the rotation
			body_temp = Rot * IF_temp;
			dx = body_temp(0) - b;
			dy = body_temp(1);
			
			body_error.x = -dy;   //-1*(sub_goal_y-y);
			body_error.y =  dx;   //(sub_goal_x-x);
			body_error.z =  0.0;
			
			if(new_path)
			{
				sub_goal_x = path.poses[path_counter].pose.position.x;
				sub_goal_y = path.poses[path_counter].pose.position.y;
				
				//Enabling RLC
				speed_control.RLC = true;
				speed_control.CMD = false;				
				speed_control.JOY = true;
				speed_control_pub_.publish(speed_control);
				
				
				double goal_dist = sqrt(pow(dx,2)+pow(dy,2));
				if (goal_dist < sub_goal_err)
				{ 
					goal_achieved = true;
					ROS_ERROR("Goal_distance is %f so the sub goal is achieved!!!!", goal_dist);
				}
				if(goal_achieved)
				{
					ROS_INFO("Sub_goal Achieved!");
					path_counter ++;
					
					ROS_WARN(" sub goal:    x:%f,    y:%f ",  sub_goal_x, sub_goal_y);
					ROS_INFO("curr pose:    x:%f,    y:%f ", x_base_orig, y_base_orig);
					
					goal_achieved = false;
				}
				
				
				if(path_counter == path_size)
				{
					new_path = false;
					
					
					//Disableing RLC
					speed_control.RLC = false;
					speed_control_pub_.publish(speed_control);
				}
				
				
				//ROS_INFO("sub_goal   x:%f,  y:%f", sub_goal_x,sub_goal_y);
				//ROS_INFO("body_orig  x:%f,  y:%f", x_base_orig,y_base_orig);
				//ROS_WARN(" ---      dx:%f, dy:%f", dx,dy); 
				//ROS_INFO("dx = %f, dy = %f", dx, dy);
				
				
				
			}
			
			//Publish Odom
			
			nav_msgs::Odometry odom;
    			odom.header.stamp = current_time;
    			odom.header.frame_id = "base_link";

    		
    			odom.pose.pose.position.x = x_base_orig;
    			odom.pose.pose.position.y = y_base_orig;
    			odom.pose.pose.position.z = 0.0;
    		
    			geometry_msgs::Quaternion odom_quat = tf::createQuaternionMsgFromYaw(th_base_orig);
    			odom.pose.pose.orientation = odom_quat;
    			testodom_pub_.publish(odom);
			//Publish Odom .
			
			speed_pub_.publish(body_error);
			ros::spinOnce();
			rate.sleep();
		}
		
	}
	
	private:
	// Node Handler
	ros::NodeHandle n_;
	//Subscribers
	ros::Subscriber SubFromPath_;
	//ros::Subscriber SubFromOdom_;
	//ros::Subscriber SubFromControllerSpeed_;
	
	//Publishers
	ros::Publisher speed_pub_;
	ros::Publisher testodom_pub_;
	ros::Publisher speed_control_pub_;
	
	
	bool goal_achieved;
	bool new_path;	
	bool set_th_bias;
	//double Vx;
	//double Vy;
	//double Vth;
	double x_base_orig;
	double y_base_orig;
	double th_base_orig;
	double th_bias;
	double sub_goal_x;
	double sub_goal_y;
	double sub_goal_err;	
	nav_msgs::Path path;
	int path_size;
	int path_counter;
	ros::Time current_time;
	ros::Time last_time;
		
	
};

int main(int argc, char **argv)
{
	ros::init(argc, argv, "path_follower");
	ros::NodeHandle node;
	
	

	PathfollowerClass path_follower(node);
	
	path_follower.run();
	
	return 0;
}
