#include "intrinsic/intrinsic.h"
#include <cstdio>

namespace telef::intrinsic{

void IntrinsicDecomposition::initialize(const uint8_t *_rgb, const uint8_t *_normal, const uint16_t *_depth, int _width, int _height)
{
	// {
	// 	int a,b;
	// 	double x;
	// 	Eigen::SparseMatrix<double> A;
	// 	A.resize(34632,34632);
	// 	std::vector<Eigen::Triplet<double> > tp;
	// 	FILE *out=fopen("../A","r");
	// 	for(int i=0;i<866278;i++)
	// 	{
	// 		fscanf(out,"%d %d %lf",&a,&b,&x);
	// 		tp.push_back(Eigen::Triplet<double>(a-1,b-1,x));
	// 	}
	// 	A.setFromTriplets(tp.begin(), tp.end());
	// 	fclose(out);
	// 	Eigen::VectorXd bb;
	// 	bb = Eigen::VectorXd::Zero(34632);
	// 	out=fopen("../b","r");
	// 	for(int i=0;i<34563;++i)
	// 	{
	// 		fscanf(out,"%d %d %lf",&a,&b,&x);
	// 		bb[b-1]=x;
	// 	}
	// 	fclose(out);
	// 	std::printf("recons\n");
	// Eigen::SimplicialLDLT<Eigen::SparseMatrix<double> > cg(A);  // performs a Cholesky factorization of A
	// //cg.compute(A);
	// Eigen::VectorXd xx = cg.solve(bb);

	// 	out=fopen("../depth/index","w");
	// 	for(int i=0;i<34632;i++)
	// 		fprintf(out,"%lf\n",xx[i]);
	// 	fclose(out);
	// }
	// std::printf("hohoho\n");
	width = _width;
	height = _height;

	color = new double[3*width*height];
	chrom = new double[3*width*height];
	points = new double[3*width*height];
	nMap = new double[3*width*height];
	vMap = new double[width*height];
	mask = new bool[width*height];
	index = new int[width*height];

	getMask(_depth);
	getPoints(_depth);
	dims = indexMapping.size();
	LLENORMAL.resize(dims,dims);
	LLENORMAL.setZero();
	LLEGRID.resize(dims,dims);
	LLEGRID.setZero();
	WRC.resize(dims,dims);
	WRC.setZero();
	WSC.resize(dims,dims);
	WSC.setZero();
	MASK.resize(dims,dims);
	MASK.setZero();
	L_S.resize(dims,dims);
	L_S.setZero();
	consVecCont = Eigen::VectorXd::Zero(dims);
	for(int i=0;i<height;++i)
		for(int j=0;j<width;++j)
		{
			if(!mask[width*i+j])
				continue;

			double nn=0.0;
			for(int k=0;k<3;k++)
			{
				color[3*width*i+3*j+k]=_rgb[3*width*i+3*j+k]/255.0;
				nMap[3*width*i+3*j+k]=_normal[3*width*i+3*j+k]/128.0-1.0;
				nn+=nMap[3*width*i+3*j+k]*nMap[3*width*i+3*j+k];
			}
			nn = sqrt(nn);
			for(int k=0;k<3;k++)
				nMap[3*width*i+3*j+k]/=nn;
		}

	if(true)
	{
		uint8_t *out = new uint8_t[width*height*3];
		for(int i=0;i<width*height*3;i++)
		{
			if(mask[i/3])
				out[i]=(uint8_t)((nMap[i]+1.0)*128.0);
		}
      pcl::io::saveCharPNGFile(
          "../depth/nMap.png",
          out,
          width,
          height,3);
      delete [] out;
    }
	getChrom();
	getVarianceMap(5);
}

void IntrinsicDecomposition::process(double *result_intensity)
{
	getGridLLEMatrix(10,6);
	getNormalConstraintMatrix(0.5);//sigma_n
	getContinuousConstraintMatrix(0.0001,0.8);//sigma_c,sigma_i
	getLaplacian();

	//A = 4 * WRC + 3 * mask1 * (spI - LLEGRID) + 3 * mask2 * (spI - LLENORMAL) + 0.025 * WSC;
	//b = 4 * consVecCont;
	Eigen::SparseMatrix<double> spI(indexMapping.size(),indexMapping.size());
	spI.setIdentity();

	//Eigen::SparseMatrix<double> A = 4 * WRC + 3 * MASK * (spI - LLEGRID) + 3 * MASK * (spI - LLENORMAL) + 1 * L_S + 0.025 * WSC;
	//Eigen::SparseMatrix<double> A = 4 * WRC + 3 * MASK * LLEGRID + 3 * MASK * LLENORMAL + 1 * L_S + 0.025 * WSC;
	Eigen::SparseMatrix<double> A = 4 * WRC + 3 * MASK * LLEGRID + 3 * MASK * LLENORMAL + 1 * L_S + 0.025 * WSC;
	Eigen::VectorXd b = 4 * consVecCont;

	std::printf("Ax=b reconstructed\n");

	if(false)
	{
		 FILE *out=fopen("../depth/A","w");
		 for(int k=0;k<A.outerSize();++k)
		 	for(Eigen::SparseMatrix<double>::InnerIterator it(A,k);it;++it)
		 		fprintf(out,"%ld %ld %lf\n",it.row(),it.col(),it.value());
		 fclose(out);

		out=fopen("../depth/b","w");
		for(int i=0;i<dims;++i)
			fprintf(out,"%lf\n",b[i]);
		fclose(out);

		out=fopen("../depth/index","w");
		for(int i=0;i<dims;i++)
			fprintf(out,"%d %d\n",indexMapping[i].first,indexMapping[i].second);
		fclose(out);
	}

	// for(int i=0;i<dims;i++)
	// 	printf("%lf\n",consVecCont[i]);

	Eigen::SimplicialLDLT<Eigen::SparseMatrix<double> > cg(A);  // performs a Cholesky factorization of A
	//cg.compute(A);
	Eigen::VectorXd x = cg.solve(b);

	std::printf("solved!\n");

	//std::cout << "#iterations:     " << cg.iterations() << std::endl;
	//std::cout << "estimated error: " << cg.error()      << std::endl;

	for(int it=0;it<indexMapping.size();it++)
	{
		int i=indexMapping[it].first;
		int j=indexMapping[it].second;
		result_intensity[i*width+j] = std::exp(x[it])/2.0;
		//std::printf("%f\t",result_intensity[i*width+j]);
	}
}

void IntrinsicDecomposition::release()
{
	delete [] color;
	delete [] chrom;
	delete [] points;
	delete [] nMap;
	delete [] vMap;
	delete [] mask;
	delete [] index;
	indexMapping.clear();

	LLENORMAL.setZero();
	LLEGRID.setZero();
	WRC.setZero();
	WSC.setZero();
	MASK.setZero();
	L_S.setZero();
	consVecCont.setZero();
	//cvReleaseSparseMat(&LLENORMAL);
	//cvReleaseSparseMat(&LLEGRID);
	//cvReleaseSparseMat(&WRC);
	//cvReleaseSparseMat(&WSC);
}

void IntrinsicDecomposition::getMask(const uint16_t *_depth)
{
	const uint16_t INVALID = 65535;

	int n=0;
	for (int i=0;i<height;i++)
		for(int j=0;j<width;j++)
		{
			mask[i*width+j] = (_depth[i*width+j]!=INVALID);
			if(mask[i*width+j])
			{
				index[i*width+j]=n++;
				indexMapping.push_back(std::make_pair(i,j));
			}
		}
}

void IntrinsicDecomposition::getPoints(const uint16_t *depth)
{
	double *is = new double[height];
	double *js = new double[width];

	for(int i=0;i<height;++i)
		is[i] = (double)(i-height/2)/height*2.0*std::tan(CV_PI/6);

	for(int j=0;j<width;++j)
		js[j] = (double)(j-width/2)/width*2.0*std::tan(CV_PI/6)*height/width;

	for(int i=0;i<height;i++)
	{
		for(int j=0;j<width;j++)
		{
			double d = depth[width*i+j]/65535.0;
			points[3*width*i+3*j+0]=d*js[j];
			points[3*width*i+3*j+1]=d*is[i];
			points[3*width*i+3*j+2]=-d;
		}
	}
	delete [] is;
	delete [] js;
}

void IntrinsicDecomposition::getChrom()
{
	for(int it=0;it<indexMapping.size();++it)
	{
		int i=indexMapping[it].first;
		int j=indexMapping[it].second;

		double intensity = 0.0;
		intensity += color[3*width*i+3*j+0]*color[3*width*i+3*j+0];
		intensity += color[3*width*i+3*j+1]*color[3*width*i+3*j+1];
		intensity += color[3*width*i+3*j+2]*color[3*width*i+3*j+2];
		if(intensity<1e-10)
			intensity=1e-10;
		intensity = sqrt(intensity);

		chrom[3*width*i+3*j+0] = color[3*width*i+3*j+0]/intensity;
		chrom[3*width*i+3*j+1] = color[3*width*i+3*j+1]/intensity;
		chrom[3*width*i+3*j+2] = color[3*width*i+3*j+2]/intensity;
	}
	if(false)
	{
		uint8_t *out = new uint8_t[width*height*3];
		for(int i=0;i<width*height*3;i++)
		{
			if(mask[i/3])
				out[i]=(uint8_t)(chrom[i]*255.0);
		}
      pcl::io::saveCharPNGFile(
          "../depth/chrom.png",
          out,
          width,
          height,3);
      delete [] out;
    }
}

void IntrinsicDecomposition::getVarianceMap(int patch_size)
{
	int p_size=patch_size/2;
	for (int i=0;i<height;i++)
	{
		for(int j=0;j<width;j++)
		{
			if(!mask[i*width+j])
				continue;
			int cnt=0;
			double p[3]={0.0,0.0,0.0},pp=0.0;
			for(int k=-p_size;k<=p_size;k++)
				for(int l=-p_size;l<=p_size;l++)
				{
					if(i+k<0 || i+k>=height || j+l<0 || j+l>=width || !mask[(i+k)*width+(j+l)])
						continue;
					for(int m=0;m<3;m++)
					{
						double var = nMap[3*(i+k)*width+3*(j+l)+m];
						p[m] += var;
						pp += var*var;
					}
					cnt++;
				}
			pp/=cnt;
			p[0]/=cnt;
			p[1]/=cnt;
			p[2]/=cnt;
			vMap[width*i+j] = pp-p[0]*p[0]-p[1]*p[1]-p[2]*p[2];
		}
	}
}

void IntrinsicDecomposition::getGridLLEMatrix(int K, int g_size)
{
	int ngrid_w = ceil(width / (double)g_size);
	int ngrid_h = ceil(height / (double)g_size);
	int Maxgrid = ngrid_w * ngrid_h;
	int *ipos = new int[Maxgrid];
	int *jpos = new int[Maxgrid];
	int Ngrid=0;

	for(int i=0;i<height;i+=g_size)
		for(int j=0;j<width;j+=g_size)
		{
			bool flag=false;
			for(int gi=0;gi<g_size && i+gi<height;gi++)
			{
				for(int gj=0;gj<g_size && j+gj<width;gj++)
					if(mask[(i+gi)*width+(j+gj)])
					{
						flag=true;
						break;
					}
				if(flag)
					break;
			}
			if(flag)
				Ngrid++;
		}

	pcl::PointCloud<pcl::PointXYZ>::Ptr cloud3d (new pcl::PointCloud<pcl::PointXYZ>);
	pcl::PointCloud<pcl::PointXYZRGB>::Ptr cloud6d (new pcl::PointCloud<pcl::PointXYZRGB>);

  	// Generate pointcloud data
  	cloud3d->width = Ngrid;
  	cloud3d->height = 1;
  	cloud3d->points.resize (cloud3d->width * cloud3d->height);

  	cloud6d->width = Ngrid;	
  	cloud6d->height = 1;
  	cloud6d->points.resize (cloud6d->width * cloud6d->height);

	//building kd-tree
  	int n=0;
	std::vector<Eigen::Triplet<double> > tp;
	for(int i=0;i<height;i+=g_size)
		for(int j=0;j<width;j+=g_size)
		{
			bool flag=true;
			double vmin = 99999999999;
			for(int gi=0;gi<g_size && i+gi<height;gi++)
				for(int gj=0;gj<g_size && j+gj<width;gj++)
				{
					if (i+gi<0 || i+gi>=height || j+gj<0 || j+gj>=width || !mask[(i+gi)*width+(j+gj)])
						continue;
					double var = vMap[(i+gi)*width+(j+gj)];
					if(mask[(i+gi)*width+(j+gj)] && var < vmin)
					{
						ipos[n] = i+gi;
						jpos[n] = j+gj;
						vmin = var;
						flag = false;
					}
				}
			if(flag)
				continue;
			//printf("%d / %d : %d %d -> %d -> %d\n",n,Ngrid,ipos[n],jpos[n],ipos[n]*width+jpos[n],index[ipos[n]*width+jpos[n]]);

			cloud3d->points[n].x = nMap[ipos[n]*width*3+jpos[n]*3+0];
			cloud3d->points[n].y = nMap[ipos[n]*width*3+jpos[n]*3+1];
			cloud3d->points[n].z = nMap[ipos[n]*width*3+jpos[n]*3+2];

			cloud6d->points[n].x = nMap[ipos[n]*width*3+jpos[n]*3+0];
			cloud6d->points[n].y = nMap[ipos[n]*width*3+jpos[n]*3+1];
			cloud6d->points[n].z = nMap[ipos[n]*width*3+jpos[n]*3+2];
			cloud6d->points[n].r = points[ipos[n]*width*3+jpos[n]*3+0];
			cloud6d->points[n].g = points[ipos[n]*width*3+jpos[n]*3+1];
			cloud6d->points[n].b = points[ipos[n]*width*3+jpos[n]*3+2];
			tp.push_back(Eigen::Triplet<double>(index[ipos[n]*width+jpos[n]],index[ipos[n]*width+jpos[n]],1));
			n++;
		}
	MASK.setFromTriplets(tp.begin(), tp.end());

	std::vector<int> pointIdxNKNSearch(K+1);
	std::vector<float> pointNKNSquaredDistance(K+1);

	//for LLENORMAL
	pcl::KdTreeFLANN<pcl::PointXYZ> kdtree3d;
	kdtree3d.setInputCloud(cloud3d);
	cv::Mat1d z(K, 3);
	int dims2[2] = {dims, dims};//{h*w,h*w};
	tp.clear();
	for(int i=0;i<Ngrid;i++)
	{
		double tol=1e-3;
		kdtree3d.nearestKSearch(cloud3d->points[i],K+1,pointIdxNKNSearch,pointNKNSquaredDistance);

		z.setTo(0);
		for(int k=0,kk=0;k<=K;++k)
		{
			int nk=pointIdxNKNSearch[k];
			if(nk==i)
				continue;
			z(kk,0) = nMap[ipos[nk]*width*3+jpos[nk]*3+0] - nMap[ipos[i]*width*3+jpos[i]*3+0];
			z(kk,1) = nMap[ipos[nk]*width*3+jpos[nk]*3+1] - nMap[ipos[i]*width*3+jpos[i]*3+1];
			z(kk,2) = nMap[ipos[nk]*width*3+jpos[nk]*3+2] - nMap[ipos[i]*width*3+jpos[i]*3+2];
			kk++;
		}

		// % local covariance
		cv::Mat1d C = z * z.t(); // C = KxK matrix

		// % regularlization (K>D)
		double t = cv::trace(C)[0];
		C = C + tol*t*cv::Mat1d::eye(K, K);

		// % solve Cw=1
		cv::Mat1d w(K, 1);
		cv::solve(C, cv::Mat1d::ones(K, 1), w);
		double ws = 0;
		for(int k=0;k<K;k++)
			ws += w(k, 1);

		// % enforce sum(w)=1
		for(int k=0,kk=0;k<=K;k++) {
			if(pointIdxNKNSearch[k]==i)
				continue;
			int p = ipos[i]*width+jpos[i];
			int q = ipos[pointIdxNKNSearch[k]]*width+jpos[pointIdxNKNSearch[k]];
			tp.push_back(Eigen::Triplet<double>(index[p],index[q],-w(kk++, 1) / ws));
			//LLENORMAL.coeffRef(index[p],index[q]) = w(k,1) /ws;
		}
	}
	for(int i=0;i<dims;++i)
		tp.push_back(Eigen::Triplet<double>(i,i,1.0));
	LLEGRID.setFromTriplets(tp.begin(), tp.end());
	std::printf("LLENORMAL\n");

	//for LLEGRID
	pcl::KdTreeFLANN<pcl::PointXYZRGB> kdtree6d;
	kdtree6d.setInputCloud(cloud6d);
	tp.clear();
	for(int i=0;i<Ngrid;i++)
	{
		double tol=1e-3;
		kdtree6d.nearestKSearch(cloud6d->points[i],K+1,pointIdxNKNSearch,pointNKNSquaredDistance);
		z.setTo(0);
		for(int k=0,kk=0;k<=K;++k)
		{
			int nk=pointIdxNKNSearch[k];
			if(nk==i)
				continue;
			z(kk,0) = nMap[ipos[nk]*width*3+jpos[nk]*3+0] - nMap[ipos[i]*width*3+jpos[i]*3+0];
			z(kk,1) = nMap[ipos[nk]*width*3+jpos[nk]*3+1] - nMap[ipos[i]*width*3+jpos[i]*3+1];
			z(kk,2) = nMap[ipos[nk]*width*3+jpos[nk]*3+2] - nMap[ipos[i]*width*3+jpos[i]*3+2];
			kk++;
		}

		// % local covariance
		cv::Mat1d C = z * z.t(); // C = KxK matrix

		// % regularlization (K>D)
		double t = cv::trace(C)[0];
		C = C + tol*t*cv::Mat1d::eye(K, K);

		// % solve Cw=1
		cv::Mat1d w(K, 1);
		cv::solve(C, cv::Mat1d::ones(K, 1), w);
		double ws = 0;
		for(int k=0;k<K;k++)
			ws += w(k, 1);

		// % enforce sum(w)=1
		for(int k=0,kk=0;k<=K;k++) {
			if(pointIdxNKNSearch[k]==i)
				continue;
			int p = ipos[i]*width+jpos[i];
			int q = ipos[pointIdxNKNSearch[k]]*width+jpos[pointIdxNKNSearch[k]];
			tp.push_back(Eigen::Triplet<double>(index[p],index[q],-w(kk++, 1) / ws));
			//LLEGRID.coeffRef(index[p],index[q]) = w(k,1) /ws;
		}
	}
	for(int i=0;i<dims;++i)
		tp.push_back(Eigen::Triplet<double>(i,i,1.0));
	LLEGRID.setFromTriplets(tp.begin(), tp.end());
	std::printf("LLEGRID\n");
	delete [] ipos;
	delete [] jpos;
}

//WSC
void IntrinsicDecomposition::getNormalConstraintMatrix(double sig_n)
{
	int nx[] = {0, 0, 1, -1, -1, 1, 1, -1};
	int ny[] = {1, -1, 0, 0, -1, 1, -1, 1};
	double np[3], nq[3];

	int dims2[2] = {dims, dims};//{h*w,h*w};
	std::vector<Eigen::Triplet<double> > tp;
	for(int it=0;it<indexMapping.size();++it)
	{
		int i=indexMapping[it].first;
		int j=indexMapping[it].second;

		if(!mask[i*width+j])
			continue;

		np[0] = nMap[3*i*width+3*j+0];
		np[1] = nMap[3*i*width+3*j+1];
		np[2] = nMap[3*i*width+3*j+2];

		for(int k=0;k<8;k++)
		{
			int qi = i + nx[k];
			int qj = j + ny[k];
			if(qi < 0 || qj < 0 || qi >= height || qj >= width || !mask[qi*width+qj])
				continue;

			nq[0] = nMap[3*qi*width+3*qj+0];
			nq[1] = nMap[3*qi*width+3*qj+1];
			nq[2] = nMap[3*qi*width+3*qj+2];

			double dist = 2.0 * (1.0 - (np[0]*nq[0]+np[1]*nq[1]+np[2]*nq[2]));	

			double weight = (exp(-dist*dist/(sig_n*sig_n)));

			if(std::isnan(weight)) weight = 0;
			int p = index[i*width+j];
			int q = index[qi*width+qj];
			tp.push_back(Eigen::Triplet<double>(p,p,weight));
			tp.push_back(Eigen::Triplet<double>(q,q,weight));
			tp.push_back(Eigen::Triplet<double>(p,q,-weight));
			tp.push_back(Eigen::Triplet<double>(q,p,-weight));
			//WSC.coeffRef(p,p) += weight;
			//WSC.coeffRef(q,q) += weight;
			//WSC.coeffRef(p,q) += -weight;
			//WSC.coeffRef(q,p) += -weight;
		}
		//std::printf("%d / %d\n",it,indexMapping.size());
	}
	WSC.setFromTriplets(tp.begin(), tp.end());
	std::printf("WSC\n");
}

//WRC
void IntrinsicDecomposition::getContinuousConstraintMatrix(double sig_c, double sig_i)
{
	int nx[] = {0, 0, 1, -1, -1, 1, 1, -1};
	int ny[] = {1, -1, 0, 0, -1, 1, -1, 1};
	double cp[3], cq[3], ip[3], iq[3];
	double lp, lq;

	int dims2[2] = {dims, dims};//{h*w,h*w};
	std::vector<Eigen::Triplet<double> > tp;
	for(int it=0;it<indexMapping.size();++it)
	{
		int i=indexMapping[it].first;
		int j=indexMapping[it].second;

		if(!mask[i*width+j])
			continue;

		cp[0] = chrom[3*i*width+3*j+0];
		cp[1] = chrom[3*i*width+3*j+1];
		cp[2] = chrom[3*i*width+3*j+2];
		ip[0] = color[3*i*width+3*j+0];
		ip[1] = color[3*i*width+3*j+1];
		ip[2] = color[3*i*width+3*j+2];
		lp = std::sqrt(ip[0]*ip[0] + ip[1]*ip[1] + ip[2]*ip[2]);
		if(lp<0.00001)
			lp = 0.00001;
		lp = std::log(lp);

		for(int k=0;k<8;k++)
		{
			int qi = i + nx[k];
			int qj = j + ny[k];
			if(qi < 0 || qj < 0 || qi >= height || qj >= width || !mask[qi*width+qj])
				continue;

			cq[0] = chrom[3*qi*width+3*qj+0];
			cq[1] = chrom[3*qi*width+3*qj+1];
			cq[2] = chrom[3*qi*width+3*qj+2];
			iq[0] = color[3*qi*width+3*qj+0];
			iq[1] = color[3*qi*width+3*qj+1];
			iq[2] = color[3*qi*width+3*qj+2];
			lq = std::sqrt(iq[0]*iq[0] + iq[1]*iq[1] + iq[2]*iq[2]);
			if(lq<0.00001)
				lq = 0.00001;
			lq = std::log(lq);

			double dist = 2.0 * (1.0 - (cp[0]*cq[0]+cp[1]*cq[1]+cp[2]*cq[2]));	
			double weight = (1 + exp(-exp(lp) * exp(lp) / (sig_i*sig_i) - exp(lq)*exp(lq) / (sig_i*sig_i)));

			weight = weight * (exp(-dist*dist/(sig_c*sig_c)));

			if(std::isnan(weight)) weight = 0;
			int p = index[i*width+j];
			int q = index[qi*width+qj];
			tp.push_back(Eigen::Triplet<double>(p,p,weight));
			tp.push_back(Eigen::Triplet<double>(q,q,weight));
			tp.push_back(Eigen::Triplet<double>(p,q,-weight));
			tp.push_back(Eigen::Triplet<double>(q,p,-weight));
			//WRC.coeffRef(p,p) += weight;
			//WRC.coeffRef(q,q) += weight;
			//WRC.coeffRef(p,q) += -weight;
			//WRC.coeffRef(q,p) += -weight;
		
			double dI = lp-lq;
			consVecCont[p] += weight * dI;
			consVecCont[q] -= weight * dI;
		}
		//std::printf("%d / %d\n",it,indexMapping.size());
	}
	WRC.setFromTriplets(tp.begin(), tp.end());
	std::printf("WRC, consVecCont\n");
}

//L_S
void IntrinsicDecomposition::getLaplacian()
{
	int nx[] = {0, 0, 0, 1, -1, -1, 1, 1, -1};
	int ny[] = {0, 1, -1, 0, 0, -1, 1, -1, 1};
	int pp[9];
	double win[9][3],tmp[9][3],mu[3],tval[9][9];
	cv::Mat1d var(3,3);

	int dims2[2] = {dims, dims};//{h*w,h*w};
	std::vector<Eigen::Triplet<double> > tp;
	std::vector<double> sumA(dims);
	for(int it=0;it<indexMapping.size();++it)
	{
		int cnt=0;
		int i=indexMapping[it].first;
		int j=indexMapping[it].second;
		mu[0]=mu[1]=mu[2]=0.0;
		for(int k=0;k<9;k++)
		{
			int qi = i + nx[k];
			int qj = j + ny[k];
			int q = qi*width+qj;
			if(qi < 0 || qj < 0 || qi >= height || qj >= width || !mask[q])
				continue;
			pp[cnt]=index[q];
			win[cnt][0]=color[3*q+0];
			win[cnt][1]=color[3*q+1];
			win[cnt][2]=color[3*q+2];
			mu[0]+=color[3*q+0];
			mu[1]+=color[3*q+1];
			mu[2]+=color[3*q+2];
			cnt++;
		}
		mu[0]/=cnt;
		mu[1]/=cnt;
		mu[2]/=cnt;

		for(int i=0;i<3;i++)
			for(int j=0;j<3;j++)
			{
				var[i][j]=0;
				for(int k=0;k<cnt;k++)
					var[i][j]+=win[k][j]*win[k][i];
				var[i][j]/=cnt;
				var[i][j]-=mu[i]*mu[j];
				if(i==j)
					var[i][j]+=1e-5;
			}
		var = var.inv();

		for(int k=0;k<cnt;k++)
		{
			win[k][0]-=mu[0];
			win[k][1]-=mu[1];
			win[k][2]-=mu[2];
		}

		for(int i=0;i<cnt;i++)
			for(int j=0;j<3;j++)
			{
				tmp[i][j]=0.0;
				for(int k=0;k<3;k++)
					tmp[i][j]+=win[i][j]*var[j][k];
			}
		for(int i=0;i<cnt;i++)
			for(int k=0;k<cnt;k++)
			{
				tval[i][k]=1.0;
				for(int j=0;j<3;j++)
					tval[i][k]+=tmp[i][j]*win[k][j];
				tval[i][k]/=cnt;
				tp.push_back(Eigen::Triplet<double>(pp[i],pp[k],-tval[i][k]));
				sumA[pp[i]]+=tval[i][k];
			}
	}
	for(int i=0;i<dims;++i)
		tp.push_back(Eigen::Triplet<double>(i,i,sumA[i]));
	L_S.setFromTriplets(tp.begin(), tp.end());
	std::printf("L_S\n");
}

}