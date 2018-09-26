#include <new_exploration_programs/centroid_matching.h>

CentroidMatching::CentroidMatching()
:centroid_merged_cloud(new pcl::PointCloud<pcl::PointXYZRGB>),
//match_merged_cloud(new pcl::PointCloud<pcl::PointXYZ>),
match_source_cloud(new pcl::PointCloud<pcl::PointXYZRGB>),
indi_merged_cloud_m(new pcl::PointCloud<pcl::PointXYZRGB>),
indi_source_cloud_m(new pcl::PointCloud<pcl::PointXYZRGB>),
icpout_cloud(new pcl::PointCloud<pcl::PointXYZRGB>),
for_merge_cloud(new pcl::PointCloud<pcl::PointXYZRGB>),
for_icpmerged_cloud(new pcl::PointCloud<pcl::PointXYZRGB>),
input_source_cloud(new pcl::PointCloud<pcl::PointXYZRGB>),
voxelizeCloud(new pcl::PointCloud<pcl::PointXYZRGB>)
{
  smi.setCallbackQueue(&mi_queue);
  smi_sub = smi.subscribe("/pointcloud_matching/matching_info",1,&CentroidMatching::input_matchinginfo,this);
  pmc_pub = pmi.advertise<sensor_msgs::PointCloud2>("centroid_matching/merged_cloud", 1);
  mids_pub = pmi.advertise<sensor_msgs::PointCloud2>("centroid_matching/middle_source_cloud", 1);
  midm_pub = pmi.advertise<sensor_msgs::PointCloud2>("centroid_matching/middle_merged_cloud", 1);
  input_info = false;
  one_matching = false;
  no_matching = false;
  //icp_matrix = Eigen::Matrix4f::Identity ();
  icp_rot_matrix = Eigen::Matrix3f::Identity ();
  icp_tra_vector = Eigen::Vector3f::Zero ();
  offset = Eigen::Vector3f::Zero ();
  rot = Eigen::Matrix3f::Identity ();
  trans = Eigen::Vector3f::Zero ();

  mrtab_pub = pmi.advertise<new_exploration_programs::twoPointcloud2>("centroid_matching/mergedRtabCloud", 1);
  mrtabM_pub = pmi.advertise<sensor_msgs::PointCloud2>("centroid_matching/mergedCloudMap", 1);
  mrtabO_pub = pmi.advertise<sensor_msgs::PointCloud2>("centroid_matching/mergedCloudObstacles", 1);

  vg.setLeafSize (0.05f, 0.05f, 0.05f);

};

void CentroidMatching::input_matchinginfo(const new_exploration_programs::matching_info::ConstPtr& mi_msg)
{
  info = *mi_msg;
  input_info = true;

  //pcl::fromROSMsg (info.source_cloud.vox_cloud, *match_source_cloud);
  //pcl::fromROSMsg (info.source_cloud.clu_cloud, *match_source_cloud);

  /*
  orig_cloud /rtabmap/cloud_map
  del_cloud /rtabmap/cloud_obstacles
  clu_cloud クラスタリング
  */




  pcl::fromROSMsg (info.source_cloud.clu_cloud, *input_source_cloud);

  //pcl::fromROSMsg (info.source_cloud.vox_cloud, *for_merge_cloud);
  pcl::fromROSMsg (info.source_cloud.orig_cloud, *for_merge_cloud);
  //std::cout << "200" << '\n';
  pcl::fromROSMsg (info.merged_cloud.orig_cloud, *centroid_merged_cloud);
  pcl::fromROSMsg (info.merged_cloud.clu_cloud, *for_icpmerged_cloud);
  //std::cout << "201" << '\n';
  //pcl::fromROSMsg (info.merged_cloud.vox_cloud,, *match_merged_cloud);

  std::cout << "input_matchinginfo" << '\n';

  std::cout << "info.matching_list.size() << " << info.matching_list.size() << '\n';

  if(info.matching_list.size() == 1)
  {
    one_matching = true;
  }
  else if(info.matching_list.size() == 0)
  {
    no_matching = true;
  }


  // std::cout << "nan_check orig merged" << '\n';
  // nan_check(*for_icpmerged_cloud);
  //
  // std::cout << "nan_check orig source" << '\n';
  // nan_check(*match_source_cloud);
  // std::cout << '\n';
}

bool CentroidMatching::is_merged_empty(void)
{

  /*マッチングリストではなくマージドが空だという条件が必要*/
  //centroid_merged_cloud

  //if(info.matching_list.size() > 0)
  if(info.merged_cloud.orig_cloud.height > 0 && info.merged_cloud.orig_cloud.width > 0)
  {
    //std::cout << "matching_list is empty" << '\n';

    std::cout << "merged_cloud is not empty" << '\n';
    return false;
  }
  else
  {
    //std::cout << "matching_list is empty" << '\n';
    std::cout << "merged_cloud is empty" << '\n';
    return true;
  }
}



void CentroidMatching::icp4allcluster(void)
{
  /*ここは全部のクラスタでicpを使うためのメイン関数です*/

  int m_num = 0;
  int s_num = 0;
  std::vector<Eigen::Matrix4f> all_icp_matrix;

  for(int i=0;i<info.matching_list.size();i++)
  {
    m_num = info.matching_list[i].merged_num;
    s_num = info.matching_list[i].source_num;
    if(one_matching)
    {
      if_onematching();
    }
    else
    {
      calc_Vangle(m_num,s_num);
    }
    moving2center(m_num,s_num);//指定したクラスタが原点になるように点群を移動
    independ_matchingcloud(m_num,s_num);

    icp_estimate(m_num,s_num,all_icp_matrix);//icpで微調整

    middle_publish();
  }

  calc_rotra(all_icp_matrix);

  final_transform();
}


void CentroidMatching::nonicp_estimate(void)
{
  /*ここは全部のクラスタでicpを使うためのメイン関数です*/

  int m_num = 0;
  int s_num = 0;
  //std::vector<Eigen::Matrix4f> all_icp_matrix;
  float temp_angle = 0;
  std::vector<float> all_temp_angle;
  float true_angle;

  //for(int i=0;i<info.matching_list.size();i++)
  for(int i=0;i<1;i++)
  {
    m_num = info.matching_list[i].merged_num;
    s_num = info.matching_list[i].source_num;

    trans[0] = info.source_cloud.clu_centroids[s_num].x - info.merged_cloud.clu_centroids[m_num].x;
    //trans[1] = 0.0;
    //trans[2] = info.source_cloud.clu_centroids[s_num].z - info.merged_cloud.clu_centroids[m_num].z;

    trans[1] = info.source_cloud.clu_centroids[s_num].y - info.merged_cloud.clu_centroids[m_num].y;
    trans[2] = 0.0;

    //offset << info.merged_cloud.clu_centroids[m_num].x,0,info.merged_cloud.clu_centroids[m_num].z;
    offset << info.merged_cloud.clu_centroids[m_num].x,info.merged_cloud.clu_centroids[m_num].y,0;

    if(one_matching)
    {
      if_onematching();
      //icp_estimate(m_num,s_num);
      //icp_transform();
      true_angle = angle_m[0];

      //icp_transform();//微調整
    }
    else
    {
      calc_Vangle(m_num,s_num);//ベクトルの内積を使って回転角度の推定

      for(int j=0;j<angle_m.size();j++)
      {
        temp_angle += angle_m[j];
      }
      temp_angle /= (float)angle_m.size();

      true_angle = temp_angle * angle_decision(i,temp_angle);

      //nonicp_transform(true_angle,m_num,s_num);//計算した角度を使って変形する
    }

    nonicp_transform(true_angle,m_num,s_num);//計算した角度を使って変形する

    //icp_transform();//微調整



    //angle_m
    // for(int i=0;i<angle_m.size();i++)
    // {
    //   temp_angle += angle_m[i];
    // }
    // temp_angle /= (float)angle_m.size();
    //
    // true_angle = temp_angle * angle_decision(i,temp_angle);

    //all_temp_angle.push_back(temp_angle);

    //moving2center(m_num,s_num);//指定したクラスタが原点になるように点群を移動
    //independ_matchingcloud(m_num,s_num);

    //icp_estimate(m_num,s_num,all_icp_matrix);//icpで微調整

    //middle_publish();
  }

  //calc_rotra(all_icp_matrix);

  //nonicp_transform();

  //final_transform();
}

void CentroidMatching::nonicp_transform(float angle,int merged_num,int source_num)
{
  /*ここはicpで推定した回転等を全体に適用する関数です*/

  //rot << cos(-angle),0,sin(-angle),0,1,0,-sin(-angle),0,cos(-angle);
  rot << cos(angle),-sin(angle),0,sin(angle),cos(angle),0,0,0,1;

  Eigen::Vector3f point;
  Eigen::Vector3f a_point;
  Eigen::Vector3f a2_point;

  for(int i=0;i<for_merge_cloud->points.size();i++)
  {
    point << for_merge_cloud->points[i].x,for_merge_cloud->points[i].y,for_merge_cloud->points[i].z;

    a_point = (rot * (point - offset - trans)) + offset;
    //a2_point = (icp_rot_matrix * a_point) + icp_tra_vector + offset;
    //a2_point = a_point + offset;
    for_merge_cloud->points[i].x = a_point(0);
    for_merge_cloud->points[i].y = a_point(1);
    for_merge_cloud->points[i].z = a_point(2);
  }

  for(int i=0;i<input_source_cloud->points.size();i++)
  {
    point << input_source_cloud->points[i].x,input_source_cloud->points[i].y,input_source_cloud->points[i].z;

    a_point = (rot * (point - offset - trans)) + offset;
    //a2_point = (icp_rot_matrix * a_point) + icp_tra_vector + offset;
    //a2_point = a_point + offset;
    input_source_cloud->points[i].x = a_point(0);
    input_source_cloud->points[i].y = a_point(1);
    input_source_cloud->points[i].z = a_point(2);
  }

}

void CentroidMatching::icp_transform(void)
{
  std::cout << "icp_setup" << '\n';
  pcl::IterativeClosestPoint<pcl::PointXYZRGB, pcl::PointXYZRGB> icp;
  // Set the input source and target
  //icp.setInputSource (del_unval_cloud);
  icp.setInputSource (for_merge_cloud);
  icp.setInputTarget (centroid_merged_cloud);
  // Set the max correspondence distance to 5cm (e.g., correspondences with higher distances will be ignored)
  //icp.setMaxCorrespondenceDistance (0.05);
  // Set the maximum number of iterations (criterion 1)
  //icp.setMaximumIterations (50);
  // Set the transformation epsilon (criterion 2)
  icp.setTransformationEpsilon (1e-8);
  // Set the euclidean distance difference epsilon (criterion 3)
  icp.setEuclideanFitnessEpsilon (1);


  std::cout << "icp_start" << '\n';
  icp.align (*icpout_cloud);


  std::cout << "has converged:" << icp.hasConverged() << " score: " << icp.getFitnessScore() << std::endl;

  *for_merge_cloud = *icpout_cloud;


  Eigen::Matrix4f icp_matrix = Eigen::Matrix4f::Identity ();
  icp_matrix = icp.getFinalTransformation ().cast<float>();

  pcl::transformPointCloud (*input_source_cloud, *icpout_cloud, icp_matrix);

  *input_source_cloud = *icpout_cloud;

  //all_icp_matrix.push_back(icp_matrix);
}

void CentroidMatching::if_onematching(void)
{

  /*ここは重心がひとつのときの回転を推定するための関数です*/
  std::vector<float> angle;
  float th;

  if(angle_m.size() > 0)
  {
    th = angle_m[0];
  }
  else
  {
    th = 0;
  }

  angle.push_back(th);

  angle_m = angle;

  std::cout << "fin << if_onematching" << '\n';

}

void CentroidMatching::calc_Vangle(int merged_num, int source_num)
{
  /*ここは渡された番号のクラスタを始点とした各クラスタへのベクトルを計算します*/
  // std::vector<Eigen::Vector3f> v_m;
  // std::vector<Eigen::Vector3f> v_s;

  // Eigen::Vector3f mc_vector;
  // Eigen::Vector3f sc_vector;


  std::vector<Eigen::Vector2f> v_m;
  std::vector<Eigen::Vector2f> v_s;

  Eigen::Vector2f mc_vector;
  Eigen::Vector2f sc_vector;

  std::vector<float> angle;
  float th;


  /*マッチングした個数-1個のベクトルを計算*/
  for(int i=0;i<info.matching_list.size();i++)
  {
    // mc_vector[0] = info.merged_cloud.clu_centroids[info.matching_list[i].merged_num].x - info.merged_cloud.clu_centroids[merged_num].x;
    // mc_vector[1] = info.merged_cloud.clu_centroids[info.matching_list[i].merged_num].y - info.merged_cloud.clu_centroids[merged_num].y;
    // mc_vector[2] = info.merged_cloud.clu_centroids[info.matching_list[i].merged_num].z - info.merged_cloud.clu_centroids[merged_num].z;
    //
    // sc_vector[0] = info.source_cloud.clu_centroids[info.matching_list[i].source_num].x - info.source_cloud.clu_centroids[source_num].x;
    // sc_vector[1] = info.source_cloud.clu_centroids[info.matching_list[i].source_num].y - info.source_cloud.clu_centroids[source_num].y;
    // sc_vector[2] = info.source_cloud.clu_centroids[info.matching_list[i].source_num].z - info.source_cloud.clu_centroids[source_num].z;

    mc_vector[0] = info.merged_cloud.clu_centroids[info.matching_list[i].merged_num].x - info.merged_cloud.clu_centroids[merged_num].x;
    //mc_vector[1] = info.merged_cloud.clu_centroids[info.matching_list[i].merged_num].z - info.merged_cloud.clu_centroids[merged_num].z;
    mc_vector[1] = info.merged_cloud.clu_centroids[info.matching_list[i].merged_num].y - info.merged_cloud.clu_centroids[merged_num].y;

    sc_vector[0] = info.source_cloud.clu_centroids[info.matching_list[i].source_num].x - info.source_cloud.clu_centroids[source_num].x;
    //sc_vector[1] = info.source_cloud.clu_centroids[info.matching_list[i].source_num].z - info.source_cloud.clu_centroids[source_num].z;
    sc_vector[1] = info.source_cloud.clu_centroids[info.matching_list[i].source_num].y - info.source_cloud.clu_centroids[source_num].y;

    if(info.matching_list[i].merged_num != merged_num)
    {
      v_m.push_back(mc_vector);
      v_s.push_back(sc_vector);

      /*回転の向きの正負を決める必要がある*/
      th = acos(mc_vector.dot(sc_vector)/(mc_vector.norm()*sc_vector.norm()));
      /*ベクトルがいろんな方向に行くのでよって正負を決めるのはこれじゃ無理かも*/
      // if(sc_vector[0] < mc_vector[0])
      // {
      //   th *= -1;
      // }

      angle.push_back(th);
    }

  }

  angle_m = angle;

  std::cout << "fin << centroid_vector" << '\n';

}

void CentroidMatching::moving2center(int merged_num, int source_num)
{
  /*ここは渡された番号のクラスタが中心になるように点群を移動する関数です*/
  trans[0] = info.source_cloud.clu_centroids[source_num].x - info.merged_cloud.clu_centroids[merged_num].x;
  trans[1] = 0.0;
  trans[2] = info.source_cloud.clu_centroids[source_num].z - info.merged_cloud.clu_centroids[merged_num].z;

  *match_source_cloud = *input_source_cloud;


  float rad;

  for(int i=0;i<angle_m.size();i++)
  {
    rad += angle_m[i];
  }

  rad /= (float)angle_m.size();//これっぽい//解決しました

  Eigen::Vector3f point;
  Eigen::Vector3f a_point;
  //Eigen::Matrix3f rot;

  rot << cos(-rad),0,sin(-rad),0,1,0,-sin(-rad),0,cos(-rad);

  /*回転するまえに回転中心が原点になるように移動しないと*/
  //Eigen::Vector3f offset;
  //offset << info.merged_cloud.clu_centroids[0].x,0,info.merged_cloud.clu_centroids[0].z;
  offset << info.merged_cloud.clu_centroids[merged_num].x,info.merged_cloud.clu_centroids[merged_num].y,info.merged_cloud.clu_centroids[merged_num].z;

  for(int i=0;i<match_source_cloud->points.size();i++)
  {
    point << match_source_cloud->points[i].x,match_source_cloud->points[i].y,match_source_cloud->points[i].z;
    //a_point = rot * point + trans;
    // a_point = (rot * (point - offset - trans)) + offset;
    a_point = (rot * (point - offset - trans));//icpやる前提の重心と大体の回転だけ合わせるやつ
    match_source_cloud->points[i].x = a_point(0);
    match_source_cloud->points[i].y = a_point(1);
    match_source_cloud->points[i].z = a_point(2);
  }

  std::cout << "fin << moving_cloud" << '\n';


  std::cout << "extract cluster for ICP" << '\n';

}

float CentroidMatching::angle_decision(int list_num, float angle)
{
  /*算出したアングルの正負を決める関数です*/
  /*原点ではない一つの重心を動かしてみて誤差が少ないほうが採用角度です*/

  Eigen::Vector3f p_vector;
  Eigen::Vector3f n_vector;

  Eigen::Vector3f c_point;

  Eigen::Vector3f m_point;
  Eigen::Vector3f s_point;

  Eigen::Matrix3f r;

  float e_p;
  float e_n;

  int c_num;


  if(list_num != 0)
  {
    c_num = 0;
  }
  else
  {
    c_num = 1;
  }

  //r << cos(-angle),0,sin(-angle),0,1,0,-sin(-angle),0,cos(-angle);
  //m_num = info.matching_list[i].merged_num;
  m_point << info.merged_cloud.clu_centroids[info.matching_list[c_num].merged_num].x,info.merged_cloud.clu_centroids[info.matching_list[c_num].merged_num].y,info.merged_cloud.clu_centroids[info.matching_list[c_num].merged_num].z;
  s_point << info.source_cloud.clu_centroids[info.matching_list[c_num].source_num].x,info.source_cloud.clu_centroids[info.matching_list[c_num].source_num].y,info.source_cloud.clu_centroids[info.matching_list[c_num].source_num].z;
  //trans
  //offset


  /*正の方向に回転したとき*/
  //r << cos(-angle),0,sin(-angle),0,1,0,-sin(-angle),0,cos(-angle);
  r << cos(angle),-sin(angle),0,sin(angle),cos(angle),0,0,0,1;

  //point << match_source_cloud->points[i].x,match_source_cloud->points[i].y,match_source_cloud->points[i].z;
  //a_point = rot * point + trans;
  // a_point = (rot * (point - offset - trans)) + offset;
  c_point = (r * (s_point - offset - trans)) + offset;//icpやる前提の重心と大体の回転だけ合わせるやつ

  p_vector = c_point - m_point;
  e_p = p_vector.norm();


  /*負の方向に回転したとき*/
  //r << cos(angle),0,sin(angle),0,1,0,-sin(angle),0,cos(angle);
  r << cos(-angle),-sin(-angle),0,sin(-angle),cos(-angle),0,0,0,1;
  //point << match_source_cloud->points[i].x,match_source_cloud->points[i].y,match_source_cloud->points[i].z;
  c_point = (r * (s_point - offset - trans)) + offset;//icpやる前提の重心と大体の回転だけ合わせるやつ

  n_vector = c_point - m_point;
  e_n = n_vector.norm();


  if(e_n >= e_p)
  {
    return 1.0;
  }
  else
  {
    return -1.0;
  }


}


void CentroidMatching::independ_matchingcloud(int merged_num, int source_num)
{
  /*引数　抽出対象のマージクラスタ番号, ソースクラスタ番号, マージドで抽出する必要があるか(false=重心移動だけ), ソースで抽出する必要があるか*/
  /*ここはicpのためにマッチングしたクラスタを独立した点群にする関数です*/
  /*またマージ側の重心が点群の原点になるようにします*/

  //それぞれ以下に格納します
  pcl::PointCloud<pcl::PointXYZRGB>::Ptr indi_merged_cloud(new pcl::PointCloud<pcl::PointXYZRGB>);
  pcl::PointCloud<pcl::PointXYZRGB>::Ptr indi_source_cloud(new pcl::PointCloud<pcl::PointXYZRGB>);

  /*マージド抽出*/
  for(int i=0;i<info.merged_cloud.clu_indices[merged_num].index.size();i++)
  {
    indi_merged_cloud -> points.push_back(for_icpmerged_cloud -> points[info.merged_cloud.clu_indices[merged_num].index[i]]);
    indi_merged_cloud -> points[i].x -= offset[0];
    indi_merged_cloud -> points[i].y -= offset[1];
    indi_merged_cloud -> points[i].z -= offset[2];
  }

  /*ソース抽出*/
  for(int i=0;i<info.source_cloud.clu_indices[source_num].index.size();i++)
  {
    indi_source_cloud -> points.push_back(match_source_cloud -> points[info.source_cloud.clu_indices[source_num].index[i]]);
  }

  /*抽出したあとに点群の設定*/
  indi_merged_cloud -> width = indi_merged_cloud -> points.size();
  indi_merged_cloud -> height = 1;
  indi_merged_cloud -> is_dense = false;

  indi_source_cloud -> width = indi_source_cloud -> points.size();
  indi_source_cloud -> height = 1;
  indi_source_cloud -> is_dense = false;


  indi_merged_cloud_m = indi_merged_cloud;
  indi_source_cloud_m = indi_source_cloud;

}

void CentroidMatching::icp_estimate(int merged_num, int source_num, std::vector<Eigen::Matrix4f>& all_icp_matrix)
{

  std::cout << "icp_setup" << '\n';
  pcl::IterativeClosestPoint<pcl::PointXYZRGB, pcl::PointXYZRGB> icp;
  // Set the input source and target
  //icp.setInputSource (del_unval_cloud);
  icp.setInputSource (indi_source_cloud_m);
  icp.setInputTarget (indi_merged_cloud_m);
  // Set the max correspondence distance to 5cm (e.g., correspondences with higher distances will be ignored)
  icp.setMaxCorrespondenceDistance (0.05);
  // Set the maximum number of iterations (criterion 1)
  icp.setMaximumIterations (50);
  // Set the transformation epsilon (criterion 2)
  icp.setTransformationEpsilon (1e-8);
  // Set the euclidean distance difference epsilon (criterion 3)
  icp.setEuclideanFitnessEpsilon (1);


  std::cout << "icp_start" << '\n';
  icp.align (*icpout_cloud);

  Eigen::Matrix4f icp_matrix = Eigen::Matrix4f::Identity ();
  icp_matrix = icp.getFinalTransformation ().cast<float>();

  all_icp_matrix.push_back(icp_matrix);

}



void CentroidMatching::calc_rotra(std::vector<Eigen::Matrix4f>& all_icp_matrix)
{
  Eigen::Matrix3f sa_rot_matrix;
  Eigen::Vector3f sa_tra_vector;

  Eigen::Matrix3f lo_rot_matrix = Eigen::Matrix3f::Zero();
  Eigen::Vector3f lo_tra_vector = Eigen::Vector3f::Zero();

  for(int i=0;i<all_icp_matrix.size();i++)
  {
    sa_rot_matrix << all_icp_matrix[i](0,0),all_icp_matrix[i](0.1),all_icp_matrix[i](0.2),all_icp_matrix[i](1,0),all_icp_matrix[i](1,1),all_icp_matrix[i](1,2),all_icp_matrix[i](2,0),all_icp_matrix[i](2,1),all_icp_matrix[i](2,2);
    sa_tra_vector << all_icp_matrix[i](0,3),all_icp_matrix[i](1,3),all_icp_matrix[i](2,3);

    lo_rot_matrix += sa_rot_matrix;
    lo_tra_vector += sa_tra_vector;
  }

  lo_rot_matrix /= (float)all_icp_matrix.size();
  lo_tra_vector /= (float)all_icp_matrix.size();

  icp_rot_matrix = lo_rot_matrix;
  icp_tra_vector = lo_tra_vector;
}


void CentroidMatching::final_transform(void)
{
  /*ここはicpで推定した回転等を全体に適用する関数です*/

  //match_source_cloudを変換
  Eigen::Vector3f point;
  Eigen::Vector3f a_point;
  Eigen::Vector3f a2_point;

  //for_merge_cloud


  for(int i=0;i<for_merge_cloud->points.size();i++)
  {
    point << for_merge_cloud->points[i].x,for_merge_cloud->points[i].y,for_merge_cloud->points[i].z;

    a_point = (rot * (point - offset - trans));
    a2_point = (icp_rot_matrix * a_point) + icp_tra_vector + offset;
    //a2_point = a_point + offset;
    for_merge_cloud->points[i].x = a2_point(0);
    for_merge_cloud->points[i].y = a2_point(1);
    for_merge_cloud->points[i].z = a2_point(2);
  }

}

void CentroidMatching::merging_cloud(void)
{
  //*centroid_merged_cloud += *match_source_cloud;
  *centroid_merged_cloud += *for_merge_cloud;

  *for_icpmerged_cloud += *input_source_cloud;
  //*centroid_merged_cloud = *for_merge_cloud;




  std::cout << "fin << merging_cloud" << '\n';
}



void CentroidMatching::publish_mergedcloud(void)
{
  centroid_merged_cloud -> width = centroid_merged_cloud -> points.size();
  centroid_merged_cloud -> height = 1;
  centroid_merged_cloud -> is_dense = false;


  //std::cout << "isdence_b" << (int)centroid_merged_cloud -> is_dense << '\n';

  pcl::toROSMsg (*centroid_merged_cloud, centroid_merged_cloud_r);

  centroid_merged_cloud_r.header = info.source_cloud.header;

  pmc_pub.publish(centroid_merged_cloud_r);
}


void CentroidMatching::publish_mergedRtab(void)
{

  /*ここで*/

  centroid_merged_cloud -> width = centroid_merged_cloud -> points.size();
  centroid_merged_cloud -> height = 1;
  centroid_merged_cloud -> is_dense = false;

  for_icpmerged_cloud -> width = for_icpmerged_cloud -> points.size();
  for_icpmerged_cloud -> height = 1;
  for_icpmerged_cloud -> is_dense = false;

  // centroid_merged_cloud
  // for_icpmerged_cloud

  pcl::toROSMsg (*centroid_merged_cloud, cloudMap);
  pcl::toROSMsg (*for_icpmerged_cloud, cloudObstacles);

  cloudMap.header = info.source_cloud.header;
  cloudObstacles.header = info.source_cloud.header;


  new_exploration_programs::twoPointcloud2 merged_rtab;
  merged_rtab.merged_cloudMap = cloudMap;
  merged_rtab.merged_cloudObstacles = cloudObstacles;

  mrtab_pub.publish(merged_rtab);
  mrtabM_pub.publish(cloudMap);
  mrtabO_pub.publish(cloudObstacles);
}


/*ここからデバッグ用関数*/


void CentroidMatching::nan_check(pcl::PointCloud<pcl::PointXYZRGB>& target_cloud)
{
  int nancount = 0;


  for(int i=0;i<target_cloud.points.size();i++)
  {
    if(isnan(target_cloud.points[i].x) || isnan(target_cloud.points[i].y) || isnan(target_cloud.points[i].z))
    {
      nancount++;
    }
  }

  if(nancount > 0)
  {
    std::cout << "find_nan << " << nancount << '\n';
  }
  else
  {
    std::cout << "no_nan" << '\n';
  }

}

void CentroidMatching::middle_publish(void)
{
  std::cout << "middle_publish" << '\n';

  sensor_msgs::PointCloud2 middle_source;
  sensor_msgs::PointCloud2 middle_merged;


  for(int i=0;i<indi_merged_cloud_m->points.size();i++)
  {
    indi_merged_cloud_m->points[i].x += 5.0;
  }

  pcl::toROSMsg (*indi_merged_cloud_m, middle_merged);
  pcl::toROSMsg (*indi_source_cloud_m, middle_source);

  middle_merged.header.frame_id = "camera_rgb_optical_frame";
  middle_source.header.frame_id = "camera_rgb_optical_frame";


  mids_pub.publish(middle_source);
  midm_pub.publish(middle_merged);
}

void CentroidMatching::centroid_vector(void)
{
  std::vector<Eigen::Vector3f> v_m;
  std::vector<Eigen::Vector3f> v_s;

  Eigen::Vector3f mc_vector;
  Eigen::Vector3f sc_vector;

  std::vector<float> angle;
  float th;

  /*マッチングした個数-1個のベクトルを計算*/
  for(int i=0;i<info.matching_list.size()-1;i++)
  {

    mc_vector[0] = info.merged_cloud.clu_centroids[info.matching_list[i+1].merged_num].x - info.merged_cloud.clu_centroids[info.matching_list[0].merged_num].x;
    mc_vector[1] = info.merged_cloud.clu_centroids[info.matching_list[i+1].merged_num].y - info.merged_cloud.clu_centroids[info.matching_list[0].merged_num].y;
    mc_vector[2] = info.merged_cloud.clu_centroids[info.matching_list[i+1].merged_num].z - info.merged_cloud.clu_centroids[info.matching_list[0].merged_num].z;
    v_m.push_back(mc_vector);

    sc_vector[0] = info.source_cloud.clu_centroids[info.matching_list[i+1].source_num].x - info.source_cloud.clu_centroids[info.matching_list[0].source_num].x;
    sc_vector[1] = info.source_cloud.clu_centroids[info.matching_list[i+1].source_num].y - info.source_cloud.clu_centroids[info.matching_list[0].source_num].y;
    sc_vector[2] = info.source_cloud.clu_centroids[info.matching_list[i+1].source_num].z - info.source_cloud.clu_centroids[info.matching_list[0].source_num].z;
    v_s.push_back(sc_vector);



    /*回転の向きの正負を決める必要がある*/
    th = acos(mc_vector.dot(sc_vector)/(mc_vector.norm()*sc_vector.norm()));

    if(sc_vector[0] < mc_vector[0])
    {
      th *= -1;
    }

    angle.push_back(th);
  }

  angle_m = angle;

  std::cout << "fin << centroid_vector" << '\n';
}

void CentroidMatching::moving_cloud(void)
{

  trans[0] = info.source_cloud.clu_centroids[info.matching_list[0].source_num].x - info.merged_cloud.clu_centroids[info.matching_list[0].merged_num].x;
  trans[1] = 0.0;
  trans[2] = info.source_cloud.clu_centroids[info.matching_list[0].source_num].z - info.merged_cloud.clu_centroids[info.matching_list[0].merged_num].z;


  /*動かす奴*/

  /*並進 trans*/
  /*回転 angle_m*/

  float rad;

  for(int i=0;i<angle_m.size();i++)
  {
    rad += angle_m[i];
  }

  rad /= (float)angle_m.size();//これっぽい//解決しました



  Eigen::Vector3f point;
  Eigen::Vector3f a_point;
  //Eigen::Matrix3f rot;

  rot << cos(-rad),0,sin(-rad),0,1,0,-sin(-rad),0,cos(-rad);

  /*回転するまえに回転中心が原点になるように移動しないと*/
  //Eigen::Vector3f offset;
  //offset << info.merged_cloud.clu_centroids[0].x,0,info.merged_cloud.clu_centroids[0].z;
  offset << info.merged_cloud.clu_centroids[info.matching_list[0].merged_num].x,info.merged_cloud.clu_centroids[info.matching_list[0].merged_num].y,info.merged_cloud.clu_centroids[info.matching_list[0].merged_num].z;

  for(int i=0;i<match_source_cloud->points.size();i++)
  {
    point << match_source_cloud->points[i].x,match_source_cloud->points[i].y,match_source_cloud->points[i].z;
    //a_point = rot * point + trans;
    // a_point = (rot * (point - offset - trans)) + offset;
    a_point = (rot * (point - offset - trans));//icpやる前提の重心と大体の回転だけ合わせるやつ
    match_source_cloud->points[i].x = a_point(0);
    match_source_cloud->points[i].y = a_point(1);
    match_source_cloud->points[i].z = a_point(2);
  }

  std::cout << "fin << moving_cloud" << '\n';


  std::cout << "extract cluster for ICP" << '\n';

  int m_num;
  int s_num;
  std::vector<Eigen::Matrix4f> all_icp_matrix;

  for(int i=0;i<info.matching_list.size();i++)
  {
    m_num = info.matching_list[i].merged_num;
    s_num = info.matching_list[i].source_num;
    independ_matchingcloud(m_num,s_num);
    icp_estimate(m_num,s_num,all_icp_matrix);//icpで微調整
  }

  calc_rotra(all_icp_matrix);

  //independ_matchingcloud(info.matching_list[0].merged_num,info.matching_list[0].source_num);
  //icp_estimate(info.matching_list[0].merged_num,info.matching_list[0].source_num);//icpで微調整
  final_transform();

}

void CentroidMatching::voxelize(void)
{
  /*ここはマージしたあとにvoxelをかける関数です*/
  vg.setInputCloud (centroid_merged_cloud);
  vg.filter (*voxelizeCloud);
  std::cout << "pointcloud_is_voxeled" << std::endl;

  *centroid_merged_cloud = *voxelizeCloud;


  vg.setInputCloud (for_icpmerged_cloud);
  vg.filter (*voxelizeCloud);

  *for_icpmerged_cloud = *voxelizeCloud;

}