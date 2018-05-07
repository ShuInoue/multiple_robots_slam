#include <ros/ros.h>
#include <ros/callback_queue.h>
#include <sensor_msgs/PointCloud2.h>
#include <pcl_ros/point_cloud.h>
#include <pcl/filters/voxel_grid.h>
#include <pcl/filters/extract_indices.h>
#include <pcl/segmentation/sac_segmentation.h>
#include <pcl/segmentation/extract_clusters.h>
#include <limits>

#include <new_exploration_programs/segmented_cloud.h>

#include <visualization_msgs/MarkerArray.h>

class FeatureMatching
{
private:
  ros::NodeHandle ssc;
	ros::NodeHandle smc;

  ros::NodeHandle psc;
	ros::NodeHandle pmc;

  ros::NodeHandle prc;


  ros::Subscriber sc_sub;
  ros::Subscriber mc_sub;

  ros::Publisher sc_pub1;
	ros::Publisher sc_pub2;
	ros::Publisher sc_pub3;
	ros::Publisher sc_pub4;

  ros::Publisher mc_pub1;
	ros::Publisher mc_pub2;
	ros::Publisher mc_pub3;
	ros::Publisher mc_pub4;

  ros::Publisher rsc_pub;
  ros::Publisher rmc_pub;
  ros::Publisher rsm_pub;

  new_exploration_programs::segmented_cloud source_cloud;
  new_exploration_programs::segmented_cloud master_cloud;


  std::vector<Eigen::Vector2i> matching_list_m;

  visualization_msgs::MarkerArray matching_line_list_m;

  float shift_position;


  pcl::PointCloud<pcl::PointXYZRGB>::Ptr master_cloud_for_shift;
  sensor_msgs::PointCloud2 shift_master_cloud;

public:
  ros::CallbackQueue sc_queue;
  ros::CallbackQueue mc_queue;
  bool input_master;
  bool input_source;
  bool changed_master;
  bool matching;
  FeatureMatching()
  :master_cloud_for_shift(new pcl::PointCloud<pcl::PointXYZRGB>)
  {
    ssc.setCallbackQueue(&sc_queue);
    smc.setCallbackQueue(&mc_queue);
		sc_sub = ssc.subscribe("/pointcloud_segmentation/source_cloud",1,&FeatureMatching::input_sourcecloud,this);
    mc_sub = smc.subscribe("/pointcloud_segmentation/master_cloud",1,&FeatureMatching::input_mastercloud,this);
    input_master = false;
    input_source = false;
    changed_master = false;

    shift_position = 3.0;
    matching = false;

    sc_pub1 = psc.advertise<sensor_msgs::PointCloud2>("source_cloud/orig_cloud", 1);
		sc_pub2 = psc.advertise<sensor_msgs::PointCloud2>("source_cloud/vox_cloud", 1);
		sc_pub3 = psc.advertise<sensor_msgs::PointCloud2>("source_cloud/del_cloud", 1);
		sc_pub4 = psc.advertise<sensor_msgs::PointCloud2>("source_cloud/clu_cloud", 1);

    mc_pub1 = pmc.advertise<sensor_msgs::PointCloud2>("master_cloud/orig_cloud", 1);
    mc_pub2 = pmc.advertise<sensor_msgs::PointCloud2>("master_cloud/vox_cloud", 1);
    mc_pub3 = pmc.advertise<sensor_msgs::PointCloud2>("master_cloud/del_cloud", 1);
    mc_pub4 = pmc.advertise<sensor_msgs::PointCloud2>("master_cloud/clu_cloud", 1);

    rsc_pub = prc.advertise<sensor_msgs::PointCloud2>("pointcloud_segmentation_registered/source_cloud", 1);
    rmc_pub = prc.advertise<sensor_msgs::PointCloud2>("pointcloud_segmentation_registered/master_cloud", 1);
    rsm_pub = prc.advertise<visualization_msgs::MarkerArray>("pointcloud_segmentation_registered/show_matching", 1);


  };

  ~FeatureMatching(){};

  void input_sourcecloud(const new_exploration_programs::segmented_cloud::ConstPtr& sc_msg);
  void input_mastercloud(const new_exploration_programs::segmented_cloud::ConstPtr& mc_msg);
  void mastercloud_test(void);
  void sourcecloud_test(void);
  void matching_calc(void);
  void show_matching(void);
  void publish_registeredcloud(void);
  void publish_matchingline(void);
  void shift_mcloud(void);
};

void FeatureMatching::input_sourcecloud(const new_exploration_programs::segmented_cloud::ConstPtr& sc_msg)
{
  source_cloud = *sc_msg;
  input_source = true;
  std::cout << "input_sourcecloud" << '\n';
}

void FeatureMatching::input_mastercloud(const new_exploration_programs::segmented_cloud::ConstPtr& mc_msg)
{
  master_cloud = *mc_msg;
  input_master = true;
  std::cout << "input_mastercloud" << '\n';
}

void FeatureMatching::mastercloud_test(void)
{
  mc_pub1.publish(master_cloud.orig_cloud);
  mc_pub2.publish(master_cloud.vox_cloud);
  mc_pub3.publish(master_cloud.del_cloud);
  mc_pub4.publish(master_cloud.clu_cloud);
}

void FeatureMatching::sourcecloud_test(void)
{
  sc_pub1.publish(source_cloud.orig_cloud);
  sc_pub2.publish(source_cloud.vox_cloud);
  sc_pub3.publish(source_cloud.del_cloud);
  sc_pub4.publish(source_cloud.clu_cloud);
}

void FeatureMatching::matching_calc(void)
{
  std::cout << "matching-process_is_started" << '\n';
  //ソースクラウド
  float diff_linearity;
	float diff_planarity;
	float diff_scattering;
	float diff_omnivariance;
	float diff_anisotropy;
	float diff_eigenentropy;
	float diff_change_of_curvature;

  float diff_vector;

  const float matching_threshold = 0.05;

  std::vector<Eigen::Vector2i> matching_list;
  Eigen::Vector2i matching_pair;//[0]master,[1]source

  //std::cout << "master_cloud.clu_indices.size() : " << master_cloud.clu_indices.size() << '\n';
  //std::cout << "source_cloud.clu_indices.size() : " << source_cloud.clu_indices.size() << '\n';

  //std::cout << "matching_success_list" << '\n';

  for(int i=0;i<master_cloud.clu_indices.size();i++)
  {
    for(int j=0;j<source_cloud.clu_indices.size();j++)
    {
      diff_linearity = master_cloud.clu_features[i].linearity - source_cloud.clu_features[j].linearity;
      diff_planarity = master_cloud.clu_features[i].planarity - source_cloud.clu_features[j].planarity;
      diff_scattering = master_cloud.clu_features[i].scattering - source_cloud.clu_features[j].scattering;
      diff_omnivariance = master_cloud.clu_features[i].omnivariance - source_cloud.clu_features[j].omnivariance;
      diff_anisotropy = master_cloud.clu_features[i].anisotropy - source_cloud.clu_features[j].anisotropy;
      diff_eigenentropy = master_cloud.clu_features[i].eigenentropy - source_cloud.clu_features[j].eigenentropy;
      diff_change_of_curvature = master_cloud.clu_features[i].change_of_curvature - source_cloud.clu_features[j].change_of_curvature;

      diff_vector = sqrt(pow(diff_linearity,2)+pow(diff_planarity,2)+pow(diff_scattering,2)+pow(diff_omnivariance,2)+pow(diff_anisotropy,2)+pow(diff_eigenentropy,2)+pow(diff_change_of_curvature,2));



      if(diff_vector < matching_threshold)
      {
        std::cout << "[matching!!] master_cloud[" << i << "] and source_cloud[" << j << "]" << '\n';
        matching_pair[0] = i;
        matching_pair[1] = j;
        matching_list.push_back(matching_pair);
      }
    }
  }
  matching_list_m = matching_list;
  std::cout << '\n';

  if(matching_list_m.size()>0)
  {
    matching = true;
  }
}

void FeatureMatching::show_matching(void)
{

  //pcl::PointCloud<pcl::PointXYZRGB>::Ptr shift_master_cloud(new pcl::PointCloud<pcl::PointXYZRGB>);
  // pcl::fromROSMsg (master_cloud.clu_cloud, *master_cloud_for_shift);
  // for(int i=0;i<master_cloud_for_shift->points.size();i++)
  // {
  //   master_cloud_for_shift->points[i].y += shift_position;
  // }
  // pcl::toROSMsg (*master_cloud_for_shift, shift_master_cloud);


  visualization_msgs::Marker matching_line;
  visualization_msgs::MarkerArray matching_line_list;

  matching_line.header.frame_id = "camera_rgb_optical_frame";
  matching_line.header.stamp = ros::Time::now();
  matching_line.ns = "matching_line";
  matching_line.type = visualization_msgs::Marker::LINE_LIST;
  matching_line.action = visualization_msgs::Marker::ADD;
  matching_line.pose.orientation.w = 1.0;
  matching_line.scale.x = 0.1;
	matching_line.color.r = 1.0f;
	matching_line.color.g = 1.0f;
	matching_line.color.b = 1.0f;
	matching_line.color.a = 1.0;
	matching_line.lifetime = ros::Duration(0.5);

  geometry_msgs::Point s_centroid;
	geometry_msgs::Point m_centroid;

  std::cout << "matching_list_m.size() : " << matching_list_m.size() << '\n';

  for(int i=0;i<matching_list_m.size();i++)
  {
    m_centroid.x = master_cloud.clu_centroids[matching_list_m[i](0)].x;
    m_centroid.y = master_cloud.clu_centroids[matching_list_m[i](0)].y - shift_position;
    m_centroid.z = master_cloud.clu_centroids[matching_list_m[i](0)].z;

    s_centroid.x = source_cloud.clu_centroids[matching_list_m[i](1)].x;
    s_centroid.y = source_cloud.clu_centroids[matching_list_m[i](1)].y;
    s_centroid.z = source_cloud.clu_centroids[matching_list_m[i](1)].z;

    matching_line.id = i;
    matching_line.points.push_back(s_centroid);
    matching_line.points.push_back(m_centroid);
    matching_line_list.markers.push_back(matching_line);
  }

  matching_line_list_m.markers = matching_line_list.markers;

}

void FeatureMatching::publish_registeredcloud(void)
{
  rsc_pub.publish(source_cloud.clu_cloud);
  rmc_pub.publish(shift_master_cloud);
  //rsm_pub.publish(matching_line_list_m);
}

void FeatureMatching::publish_matchingline(void)
{
  rsm_pub.publish(matching_line_list_m);
}

void FeatureMatching::shift_mcloud(void)
{
  pcl::fromROSMsg (master_cloud.clu_cloud, *master_cloud_for_shift);
  for(int i=0;i<master_cloud_for_shift->points.size();i++)
  {
    master_cloud_for_shift->points[i].y -= shift_position;
  }
  pcl::toROSMsg (*master_cloud_for_shift, shift_master_cloud);
}

int main(int argc, char** argv)
{
	ros::init(argc, argv, "feature_matching");
	FeatureMatching fm;

  while(ros::ok())
  {
    fm.mc_queue.callOne(ros::WallDuration(1));
    if(fm.input_master)
    {
      while(ros::ok())
      {
          fm.sc_queue.callOne(ros::WallDuration(1));
          if(fm.input_source)
          {
            //fm.mastercloud_test();
            //fm.sourcecloud_test();
            fm.matching_calc();
            fm.shift_mcloud();
            if(fm.matching)
            {
              fm.show_matching();
              fm.publish_matchingline();
              fm.publish_registeredcloud();
              fm.matching = false;
            }
            else
            {
              fm.publish_registeredcloud();
            }
            fm.input_source = false;

            if(fm.changed_master)
            {
                break;
            }
          }
          else
          {
            std::cout << "source_is_nothing" << '\n';
          }
      }
      fm.input_master = false;
    }
    else
    {
      std::cout << "master_is_nothing" << '\n';
    }
	}
	return 0;
}