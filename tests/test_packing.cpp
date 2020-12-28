// Tencent is pleased to support the open source community by making ncnn available.
//
// Copyright (C) 2020 THL A29 Limited, a Tencent company. All rights reserved.
// Modifications Copyright (C) 2020 TANCOM SOFTWARE SOLUTIONS Ltd. All rights reserved.
//
// Licensed under the BSD 3-Clause License (the "License"); you may not use this file except
// in compliance with the License. You may obtain a copy of the License at
//
// https://opensource.org/licenses/BSD-3-Clause
//
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

#include "layer/packing.h"
#include "testutil.h"

#if NCNN_CUDA

static int test_packing_gpu_fp32(const ncnn::Mat& a, int in_elempack, int out_elempack)
{
    ncnn::ParamDict pd;
    pd.set(0, out_elempack);

    std::vector<ncnn::Mat> weights(0);

    ncnn::Option opt;
    opt.num_threads = 1;
    opt.use_vulkan_compute = false;
    opt.use_int8_inference = false;
    opt.use_fp16_storage = false;
    opt.use_fp16_arithmetic = false;
    opt.use_packing_layout = false;

    std::shared_ptr<ncnn::CudaAllocator> cuda_allocator = ncnn::get_current_gpu_allocator();

    ncnn::Layer* op = ncnn::create_layer("Packing");

    op->load_param(pd);

    ncnn::ModelBinFromMatArray mb(weights.data());

    op->load_model(mb);

    op->create_pipeline(opt);

    //CPU

    ncnn::Mat ap_cpu;
    ncnn::convert_packing(a, ap_cpu, in_elempack);
    ncnn::Mat b_cpu;
    auto begin_cpu = std::chrono::high_resolution_clock::now();
    ((ncnn::Packing*)op)->ncnn::Packing::forward(ap_cpu, b_cpu, opt);
    auto end_cpu = std::chrono::high_resolution_clock::now();
    std::cout << "test_packing_cpu_fp32 execution time: " << std::chrono::duration_cast<std::chrono::microseconds>(end_cpu-begin_cpu).count() << " us" << std::endl;
    ncnn::Mat c_cpu;
    op->forward(ap_cpu, c_cpu, opt);

    //GPU
    ncnn::CudaMat ap_gpu;
    ncnn::CudaMat a_gpu{a, cuda_allocator};
    ncnn::convert_packing(a_gpu, ap_gpu, in_elempack);
    ncnn::CudaMat b_gpu;
    auto begin = std::chrono::high_resolution_clock::now();
    ((ncnn::Packing*)op)->forward(ap_gpu, b_gpu, opt);
    cudaDeviceSynchronize();
    auto end = std::chrono::high_resolution_clock::now();
    std::cout << "test_packing_gpu_fp32 execution time: " << std::chrono::duration_cast<std::chrono::microseconds>(end-begin).count() << " us" << std::endl;
    ncnn::Mat b{b_gpu};
    ncnn::CudaMat c_gpu;
    op->forward(ap_gpu, c_gpu, opt);
    ncnn::Mat c{c_gpu};

    cudaDeviceSynchronize();
    checkCudaErrors(cudaGetLastError());

    op->destroy_pipeline(opt);
    delete op;

    ncnn::Mat ap{ap_gpu};

    if (CompareMat(ap_cpu, ap, 0.001) != 0)
    {
        fprintf(stderr, "test_packing_gpu_fp32 check 0 failed a.dims=%d ap_cpu=(%d %d %d) in_elempack=%d out_elempack=%d\n", ap_cpu.dims,
                ap_cpu.w, ap_cpu.h, ap_cpu.c, in_elempack, out_elempack);
        std::cout << "Input ap_cpu:" << std::endl;
        ncnn::Mat::print_mat(ap_cpu);
        std::cout << "Input ap:" << std::endl;
        ncnn::Mat::print_mat(ap);
        return -1;
    }

    if (CompareMat(b_cpu, b, 0.001) != 0)
    {
        fprintf(stderr, "test_packing_gpu_fp32 check 1 failed a.dims=%d a=(%d %d %d) in_elempack=%d out_elempack=%d\n", a.dims, a.w, a.h, a.c, in_elempack, out_elempack);
        std::cout << "Input b_cpu:" << std::endl;
        ncnn::Mat::print_mat(b_cpu);
        std::cout << "Input b:" << std::endl;
        ncnn::Mat::print_mat(b);
        return -1;
    }

    if (CompareMat(c_cpu, c, 0.001) != 0)
    {
        fprintf(stderr, "test_packing_gpu_fp32 check 2 failed a.dims=%d a=(%d %d %d) in_elempack=%d out_elempack=%d\n", a.dims, a.w, a.h, a.c, in_elempack, out_elempack);
        return -1;
    }

    if (CompareMat(b, c, 0.001) != 0)
    {
        fprintf(stderr, "test_packing_gpu_fp32 check 3 failed a.dims=%d a=(%d %d %d) in_elempack=%d out_elempack=%d\n", a.dims, a.w, a.h, a.c, in_elempack, out_elempack);
        return -1;
    }

    return 0;
}

static int test_packing_gpu_fp16(const ncnn::Mat& a, int in_elempack, int out_elempack)
{
    ncnn::ParamDict pd;
    pd.set(0, out_elempack);

    std::vector<ncnn::Mat> weights(0);

    ncnn::Option opt;
    opt.num_threads = 1;
    opt.use_vulkan_compute = false;
    opt.use_int8_inference = false;
    opt.use_fp16_storage = true;
    opt.use_fp16_arithmetic = true;
    opt.use_packing_layout = false;

    ncnn::Layer* op = ncnn::create_layer("Packing");

    if (!op->support_fp16_storage)
    {
        delete op;
        return 0;
    }

    op->load_param(pd);

    ncnn::ModelBinFromMatArray mb(weights.data());

    op->load_model(mb);

    op->create_pipeline(opt);

    std::shared_ptr<ncnn::CudaAllocator> cuda_allocator = ncnn::get_current_gpu_allocator();


    ncnn::Mat a16;
    ncnn::cast_float32_to_float16(a, a16);
    ncnn::Mat ap;
    ncnn::convert_packing(a16, ap, in_elempack);
    ncnn::Mat b_cpu;
    auto begin_cpu = std::chrono::high_resolution_clock::now();
    ((ncnn::Packing*)op)->ncnn::Packing::forward(ap, b_cpu, opt);
    auto end_cpu = std::chrono::high_resolution_clock::now();
    std::cout << "test_packing_cpu_fp16 execution time: " << std::chrono::duration_cast<std::chrono::microseconds>(end_cpu-begin_cpu).count() << " us" << std::endl;
    ncnn::Mat c_cpu;
    op->forward(ap, c_cpu, opt);
    ncnn::Mat c32_cpu;
    ncnn::cast_float16_to_float32(c_cpu, c32_cpu);




    //GPU

    ncnn::CudaMat a_gpu{a, cuda_allocator};
    ncnn::CudaMat a16_gpu;
    ncnn::cast_float32_to_float16(a_gpu, a16_gpu);
    ncnn::CudaMat ap_gpu;
    ncnn::convert_packing(a16_gpu, ap_gpu, in_elempack);
    ncnn::CudaMat b_gpu;
    auto begin = std::chrono::high_resolution_clock::now();
    ((ncnn::Packing*)op)->ncnn::Packing::forward(ap_gpu, b_gpu, opt);
    cudaDeviceSynchronize();
    auto end = std::chrono::high_resolution_clock::now();
    std::cout << "test_packing_gpu_fp16 execution time: " << std::chrono::duration_cast<std::chrono::microseconds>(end-begin).count() << " us" << std::endl;
    ncnn::CudaMat c_gpu;
    op->forward(ap_gpu, c_gpu, opt);

    cudaDeviceSynchronize();
    checkCudaErrors(cudaGetLastError());


    op->destroy_pipeline(opt);

    delete op;

    ncnn::CudaMat c32_gpu;
    ncnn::cast_float16_to_float32(c_gpu, c32_gpu);

    ncnn::Mat c32{c32_gpu};
    ncnn::Mat b{b_gpu};

    if (CompareMat(b, b_cpu, 0.001) != 0)
    {
        fprintf(stderr, "test_packing_gpu_fp16 failed a.dims=%d a=(%d %d %d) in_elempack=%d out_elempack=%d\n", a.dims, a.w, a.h, a.c, in_elempack, out_elempack);
        return -1;
    }

    if (CompareMat(c32, c32_cpu, 0.001) != 0)
    {
        fprintf(stderr, "test_packing_gpu_fp16 failed a.dims=%d a=(%d %d %d) in_elempack=%d out_elempack=%d\n", a.dims, a.w, a.h, a.c, in_elempack, out_elempack);
        return -1;
    }

    if (CompareMat(b, c32, 0.001) != 0)
    {
        fprintf(stderr, "test_packing_gpu_fp16 failed a.dims=%d a=(%d %d %d) in_elempack=%d out_elempack=%d\n", a.dims, a.w, a.h, a.c, in_elempack, out_elempack);
        return -1;
    }

    return 0;
}

#endif

static int test_packing_cpu_fp32(const ncnn::Mat& a, int in_elempack, int out_elempack)
{
    ncnn::ParamDict pd;
    pd.set(0, out_elempack);

    std::vector<ncnn::Mat> weights(0);

    ncnn::Option opt;
    opt.num_threads = 1;
    opt.use_vulkan_compute = false;
    opt.use_int8_inference = false;
    opt.use_fp16_storage = false;
    opt.use_fp16_arithmetic = false;
    opt.use_packing_layout = false;

    ncnn::Layer* op = ncnn::create_layer("Packing");

    op->load_param(pd);

    ncnn::ModelBinFromMatArray mb(weights.data());

    op->load_model(mb);

    op->create_pipeline(opt);

    ncnn::Mat ap;
    ncnn::convert_packing(a, ap, in_elempack);

    ncnn::Mat b;
    ((ncnn::Packing*)op)->ncnn::Packing::forward(ap, b, opt);

    ncnn::Mat c;
    op->forward(ap, c, opt);

    op->destroy_pipeline(opt);

    delete op;

    if (CompareMat(b, c, 0.001) != 0)
    {
        fprintf(stderr, "test_packing_cpu_fp32 failed a.dims=%d a=(%d %d %d) in_elempack=%d out_elempack=%d\n", a.dims, a.w, a.h, a.c, in_elempack, out_elempack);
        return -1;
    }

    return 0;
}

static int test_packing_cpu_fp16(const ncnn::Mat& a, int in_elempack, int out_elempack)
{
    ncnn::ParamDict pd;
    pd.set(0, out_elempack);

    std::vector<ncnn::Mat> weights(0);

    ncnn::Option opt;
    opt.num_threads = 1;
    opt.use_vulkan_compute = false;
    opt.use_int8_inference = false;
    opt.use_fp16_storage = true;
    opt.use_fp16_arithmetic = true;
    opt.use_packing_layout = false;

    ncnn::Layer* op = ncnn::create_layer("Packing");

    if (!op->support_fp16_storage)
    {
        delete op;
        return 0;
    }

    op->load_param(pd);

    ncnn::ModelBinFromMatArray mb(weights.data());

    op->load_model(mb);

    op->create_pipeline(opt);

    ncnn::Mat a16;
    ncnn::cast_float32_to_float16(a, a16);

    ncnn::Mat ap;
    ncnn::convert_packing(a16, ap, in_elempack);

    ncnn::Mat b;
    ((ncnn::Packing*)op)->ncnn::Packing::forward(ap, b, opt);

    ncnn::Mat c;
    op->forward(ap, c, opt);

    op->destroy_pipeline(opt);

    delete op;

    ncnn::Mat c32;
    ncnn::cast_float16_to_float32(c, c32);

    if (CompareMat(b, c32, 0.001) != 0)
    {
        fprintf(stderr, "test_packing_cpu_fp16 failed a.dims=%d a=(%d %d %d) in_elempack=%d out_elempack=%d\n", a.dims, a.w, a.h, a.c, in_elempack, out_elempack);
        return -1;
    }

    return 0;
}

static int test_packing_cpu(const ncnn::Mat& a, int in_elempack, int out_elempack)
{
    return 0
           || test_packing_cpu_fp32(a, in_elempack, out_elempack)
           || test_packing_cpu_fp16(a, in_elempack, out_elempack)
        ;
}

#if NCNN_CUDA
static int test_packing_gpu(const ncnn::Mat& a, int in_elempack, int out_elempack)
{
    return 0
           || test_packing_gpu_fp32(a, in_elempack, out_elempack)
           || test_packing_gpu_fp16(a, in_elempack, out_elempack)
        ;
}
#endif //NCNN_CUDA

#if NCNN_VULKAN
#include "layer/vulkan/packing_vulkan.h"

static int test_packing_gpu_buffer(const ncnn::Mat& a, int in_elempack, int out_elempack)
{
    ncnn::ParamDict pd;
    pd.set(0, out_elempack);
    pd.set(2, 1); // cast_type_from
    pd.set(3, 1); // cast_type_to
    pd.set(4, 0); // storage_type_from
    pd.set(5, 0); // storage_type_to

    std::vector<ncnn::Mat> weights(0);

    ncnn::Option opt;
    opt.num_threads = 1;
    opt.use_vulkan_compute = true;
    opt.use_int8_inference = false;
    opt.use_fp16_packed = false;
    opt.use_fp16_storage = false;
    opt.use_fp16_arithmetic = false;
    opt.use_int8_storage = false;
    opt.use_int8_arithmetic = false;
    opt.use_packing_layout = true;
    opt.use_shader_pack8 = true;
    opt.use_image_storage = false;

    ncnn::VulkanDevice* vkdev = ncnn::get_gpu_device();

    ncnn::VkAllocator* blob_vkallocator = vkdev->acquire_blob_allocator();
    ncnn::VkAllocator* staging_vkallocator = vkdev->acquire_staging_allocator();

    opt.blob_vkallocator = blob_vkallocator;
    opt.workspace_vkallocator = blob_vkallocator;
    opt.staging_vkallocator = staging_vkallocator;

    if (!vkdev->info.support_fp16_packed) opt.use_fp16_packed = false;
    if (!vkdev->info.support_fp16_storage) opt.use_fp16_storage = false;

    ncnn::Layer* op = ncnn::create_layer("Packing");

    op->vkdev = vkdev;

    op->load_param(pd);

    ncnn::ModelBinFromMatArray mb(weights.data());

    op->load_model(mb);

    op->create_pipeline(opt);

    ncnn::Mat ap;
    ncnn::convert_packing(a, ap, in_elempack);

    ncnn::Mat b;
    ((ncnn::Packing*)op)->ncnn::Packing::forward(ap, b, opt);

    ncnn::Mat d;

    // forward
    ncnn::VkCompute cmd(vkdev);

    // upload
    ncnn::VkMat a_gpu;
    cmd.record_clone(ap, a_gpu, opt);

    ncnn::VkMat d_gpu;
    op->forward(a_gpu, d_gpu, cmd, opt);

    // download
    cmd.record_clone(d_gpu, d, opt);

    cmd.submit_and_wait();

    op->destroy_pipeline(opt);

    delete op;

    vkdev->reclaim_blob_allocator(blob_vkallocator);
    vkdev->reclaim_staging_allocator(staging_vkallocator);

    if (CompareMat(b, d, 0.001) != 0)
    {
        fprintf(stderr, "test_packing_gpu_buffer failed a.dims=%d a=(%d %d %d) in_elempack=%d out_elempack=%d\n", a.dims, a.w, a.h, a.c, in_elempack, out_elempack);
        return -1;
    }

    return 0;
}

static int test_packing_gpu_image(const ncnn::Mat& a, int in_elempack, int out_elempack)
{
    ncnn::ParamDict pd;
    pd.set(0, out_elempack);
    pd.set(2, 1); // cast_type_from
    pd.set(3, 1); // cast_type_to
    pd.set(4, 1); // storage_type_from
    pd.set(5, 1); // storage_type_to

    std::vector<ncnn::Mat> weights(0);

    ncnn::Option opt;
    opt.num_threads = 1;
    opt.use_vulkan_compute = true;
    opt.use_int8_inference = false;
    opt.use_fp16_packed = false;
    opt.use_fp16_storage = false;
    opt.use_fp16_arithmetic = false;
    opt.use_int8_storage = false;
    opt.use_int8_arithmetic = false;
    opt.use_packing_layout = true;
    opt.use_shader_pack8 = true;
    opt.use_image_storage = true;

    ncnn::VulkanDevice* vkdev = ncnn::get_gpu_device();

    ncnn::VkAllocator* blob_vkallocator = vkdev->acquire_blob_allocator();
    ncnn::VkAllocator* staging_vkallocator = vkdev->acquire_staging_allocator();

    opt.blob_vkallocator = blob_vkallocator;
    opt.workspace_vkallocator = blob_vkallocator;
    opt.staging_vkallocator = staging_vkallocator;

    if (!vkdev->info.support_fp16_packed) opt.use_fp16_packed = false;
    if (!vkdev->info.support_fp16_storage) opt.use_fp16_storage = false;

    ncnn::Layer* op = ncnn::create_layer("Packing");

    op->vkdev = vkdev;

    op->load_param(pd);

    ncnn::ModelBinFromMatArray mb(weights.data());

    op->load_model(mb);

    op->create_pipeline(opt);

    ncnn::Mat ap;
    ncnn::convert_packing(a, ap, in_elempack);

    ncnn::Mat b;
    ((ncnn::Packing*)op)->ncnn::Packing::forward(ap, b, opt);

    ncnn::Mat d;

    // forward
    ncnn::VkCompute cmd(vkdev);

    // upload
    ncnn::VkImageMat a_gpu;
    cmd.record_clone(ap, a_gpu, opt);

    ncnn::VkImageMat d_gpu;
    op->forward(a_gpu, d_gpu, cmd, opt);

    // download
    cmd.record_clone(d_gpu, d, opt);

    cmd.submit_and_wait();

    op->destroy_pipeline(opt);

    delete op;

    vkdev->reclaim_blob_allocator(blob_vkallocator);
    vkdev->reclaim_staging_allocator(staging_vkallocator);

    if (CompareMat(b, d, 0.001) != 0)
    {
        fprintf(stderr, "test_packing_gpu_image failed a.dims=%d a=(%d %d %d) in_elempack=%d out_elempack=%d\n", a.dims, a.w, a.h, a.c, in_elempack, out_elempack);
        return -1;
    }

    return 0;
}

static int test_packing_gpu_buffer2image(const ncnn::Mat& a, int in_elempack, int out_elempack)
{
    ncnn::ParamDict pd;
    pd.set(0, out_elempack);
    pd.set(2, 1); // cast_type_from
    pd.set(3, 1); // cast_type_to
    pd.set(4, 0); // storage_type_from
    pd.set(5, 1); // storage_type_to

    std::vector<ncnn::Mat> weights(0);

    ncnn::Option opt;
    opt.num_threads = 1;
    opt.use_vulkan_compute = true;
    opt.use_int8_inference = false;
    opt.use_fp16_packed = false;
    opt.use_fp16_storage = false;
    opt.use_fp16_arithmetic = false;
    opt.use_int8_storage = false;
    opt.use_int8_arithmetic = false;
    opt.use_packing_layout = true;
    opt.use_shader_pack8 = true;
    opt.use_image_storage = true;

    ncnn::VulkanDevice* vkdev = ncnn::get_gpu_device();

    ncnn::VkAllocator* blob_vkallocator = vkdev->acquire_blob_allocator();
    ncnn::VkAllocator* staging_vkallocator = vkdev->acquire_staging_allocator();

    opt.blob_vkallocator = blob_vkallocator;
    opt.workspace_vkallocator = blob_vkallocator;
    opt.staging_vkallocator = staging_vkallocator;

    if (!vkdev->info.support_fp16_packed) opt.use_fp16_packed = false;
    if (!vkdev->info.support_fp16_storage) opt.use_fp16_storage = false;

    ncnn::Packing_vulkan* op = new ncnn::Packing_vulkan;

    op->vkdev = vkdev;

    op->load_param(pd);

    ncnn::ModelBinFromMatArray mb(weights.data());

    op->load_model(mb);

    op->create_pipeline(opt);

    ncnn::Mat ap;
    ncnn::convert_packing(a, ap, in_elempack);

    ncnn::Mat b;
    ((ncnn::Packing*)op)->ncnn::Packing::forward(ap, b, opt);

    ncnn::Mat d;

    // forward
    ncnn::VkCompute cmd(vkdev);

    // upload
    ncnn::VkMat a_gpu;
    cmd.record_clone(ap, a_gpu, opt);

    ncnn::VkImageMat d_gpu;
    op->forward(a_gpu, d_gpu, cmd, opt);

    // download
    cmd.record_clone(d_gpu, d, opt);

    cmd.submit_and_wait();

    op->destroy_pipeline(opt);

    delete op;

    vkdev->reclaim_blob_allocator(blob_vkallocator);
    vkdev->reclaim_staging_allocator(staging_vkallocator);

    if (CompareMat(b, d, 0.001) != 0)
    {
        fprintf(stderr, "test_packing_gpu_buffer2image failed a.dims=%d a=(%d %d %d) in_elempack=%d out_elempack=%d\n", a.dims, a.w, a.h, a.c, in_elempack, out_elempack);
        return -1;
    }

    return 0;
}

static int test_packing_gpu_image2buffer(const ncnn::Mat& a, int in_elempack, int out_elempack)
{
    ncnn::ParamDict pd;
    pd.set(0, out_elempack);
    pd.set(2, 1); // cast_type_from
    pd.set(3, 1); // cast_type_to
    pd.set(4, 1); // storage_type_from
    pd.set(5, 0); // storage_type_to

    std::vector<ncnn::Mat> weights(0);

    ncnn::Option opt;
    opt.num_threads = 1;
    opt.use_vulkan_compute = true;
    opt.use_int8_inference = false;
    opt.use_fp16_packed = false;
    opt.use_fp16_storage = false;
    opt.use_fp16_arithmetic = false;
    opt.use_int8_storage = false;
    opt.use_int8_arithmetic = false;
    opt.use_packing_layout = true;
    opt.use_shader_pack8 = true;
    opt.use_image_storage = true;

    ncnn::VulkanDevice* vkdev = ncnn::get_gpu_device();

    ncnn::VkAllocator* blob_vkallocator = vkdev->acquire_blob_allocator();
    ncnn::VkAllocator* staging_vkallocator = vkdev->acquire_staging_allocator();

    opt.blob_vkallocator = blob_vkallocator;
    opt.workspace_vkallocator = blob_vkallocator;
    opt.staging_vkallocator = staging_vkallocator;

    if (!vkdev->info.support_fp16_packed) opt.use_fp16_packed = false;
    if (!vkdev->info.support_fp16_storage) opt.use_fp16_storage = false;

    ncnn::Packing_vulkan* op = new ncnn::Packing_vulkan;

    op->vkdev = vkdev;

    op->load_param(pd);

    ncnn::ModelBinFromMatArray mb(weights.data());

    op->load_model(mb);

    op->create_pipeline(opt);

    ncnn::Mat ap;
    ncnn::convert_packing(a, ap, in_elempack);

    ncnn::Mat b;
    ((ncnn::Packing*)op)->ncnn::Packing::forward(ap, b, opt);

    ncnn::Mat d;

    // forward
    ncnn::VkCompute cmd(vkdev);

    // upload
    ncnn::VkImageMat a_gpu;
    cmd.record_clone(ap, a_gpu, opt);

    ncnn::VkMat d_gpu;
    op->forward(a_gpu, d_gpu, cmd, opt);

    // download
    cmd.record_clone(d_gpu, d, opt);

    cmd.submit_and_wait();

    op->destroy_pipeline(opt);

    delete op;

    vkdev->reclaim_blob_allocator(blob_vkallocator);
    vkdev->reclaim_staging_allocator(staging_vkallocator);

    if (CompareMat(b, d, 0.001) != 0)
    {
        fprintf(stderr, "test_packing_gpu_image2buffer failed a.dims=%d a=(%d %d %d) in_elempack=%d out_elempack=%d\n", a.dims, a.w, a.h, a.c, in_elempack, out_elempack);
        return -1;
    }

    return 0;
}
#endif

static int test_packing_0()
{
    ncnn::Mat a = RandomMat(9, 10, 16);
    ncnn::Mat b = RandomMat(9, 10, 3);

    return 0
           || test_packing_cpu(a, 1, 1)
           || test_packing_cpu(a, 4, 4)
           || test_packing_cpu(a, 4, 8)
           || test_packing_cpu(a, 1, 4)
           || test_packing_cpu(a, 4, 1)
           || test_packing_cpu(a, 1, 8)
           || test_packing_cpu(a, 8, 1)
           || test_packing_cpu(a, 4, 8)
           || test_packing_cpu(a, 8, 4)
           || test_packing_cpu(b, 1, 1)
           || test_packing_cpu(b, 4, 4)
           || test_packing_cpu(b, 4, 8)
           || test_packing_cpu(b, 1, 4)
           || test_packing_cpu(b, 4, 1)
           || test_packing_cpu(b, 1, 8)
           || test_packing_cpu(b, 8, 1)
           || test_packing_cpu(b, 4, 8)
           || test_packing_cpu(b, 8, 4)
#if NCNN_VULKAN
           || test_packing_gpu_buffer(a, 1, 1)
           || test_packing_gpu_buffer(a, 4, 4)
           || test_packing_gpu_buffer(a, 8, 8)
           || test_packing_gpu_buffer(a, 1, 4)
           || test_packing_gpu_buffer(a, 4, 1)
           || test_packing_gpu_buffer(a, 1, 8)
           || test_packing_gpu_buffer(a, 8, 1)
           || test_packing_gpu_buffer(a, 4, 8)
           || test_packing_gpu_buffer(a, 8, 4)
           || test_packing_gpu_image(a, 1, 1)
           || test_packing_gpu_image(a, 4, 4)
           || test_packing_gpu_image(a, 8, 8)
           || test_packing_gpu_image(a, 1, 4)
           || test_packing_gpu_image(a, 4, 1)
           || test_packing_gpu_image(a, 1, 8)
           || test_packing_gpu_image(a, 8, 1)
           || test_packing_gpu_image(a, 4, 8)
           || test_packing_gpu_image(a, 8, 4)
           || test_packing_gpu_buffer2image(a, 1, 1)
           || test_packing_gpu_buffer2image(a, 4, 4)
           || test_packing_gpu_buffer2image(a, 8, 8)
           || test_packing_gpu_buffer2image(a, 1, 4)
           || test_packing_gpu_buffer2image(a, 4, 1)
           || test_packing_gpu_buffer2image(a, 1, 8)
           || test_packing_gpu_buffer2image(a, 8, 1)
           || test_packing_gpu_buffer2image(a, 4, 8)
           || test_packing_gpu_buffer2image(a, 8, 4)
           || test_packing_gpu_image2buffer(a, 1, 1)
           || test_packing_gpu_image2buffer(a, 4, 4)
           || test_packing_gpu_image2buffer(a, 8, 8)
           || test_packing_gpu_image2buffer(a, 1, 4)
           || test_packing_gpu_image2buffer(a, 4, 1)
           || test_packing_gpu_image2buffer(a, 1, 8)
           || test_packing_gpu_image2buffer(a, 8, 1)
           || test_packing_gpu_image2buffer(a, 4, 8)
           || test_packing_gpu_image2buffer(a, 8, 4)
#endif // NCNN_VULKAN
#if NCNN_CUDA
              || test_packing_gpu(a, 1, 1)
              || test_packing_gpu(a, 4, 4)
              || test_packing_gpu(a, 4, 8)
              || test_packing_gpu(a, 1, 4)
              || test_packing_gpu(a, 4, 1)
              || test_packing_gpu(a, 1, 8)
              || test_packing_gpu(a, 8, 1)
              || test_packing_gpu(a, 4, 8)
              || test_packing_gpu(a, 8, 4)
              || test_packing_gpu(b, 1, 1)
              || test_packing_gpu(b, 4, 4)
              || test_packing_gpu(b, 4, 8)
              || test_packing_gpu(b, 1, 4)
              || test_packing_gpu(b, 4, 1)
              || test_packing_gpu(b, 1, 8)
              || test_packing_gpu(b, 8, 1)
              || test_packing_gpu(b, 4, 8)
              || test_packing_gpu(b, 8, 4)
#endif // NCNN_CUDA
           ;
}

static int test_packing_1()
{
    ncnn::Mat a = RandomMat(9, 16);

    return 0
           || test_packing_cpu(a, 1, 1)
           || test_packing_cpu(a, 4, 4)
           || test_packing_cpu(a, 4, 8)
           || test_packing_cpu(a, 1, 4)
           || test_packing_cpu(a, 4, 1)
           || test_packing_cpu(a, 1, 8)
           || test_packing_cpu(a, 8, 1)
           || test_packing_cpu(a, 4, 8)
           || test_packing_cpu(a, 8, 4)
#if NCNN_VULKAN
           || test_packing_gpu_buffer(a, 1, 1)
           || test_packing_gpu_buffer(a, 4, 4)
           || test_packing_gpu_buffer(a, 8, 8)
           || test_packing_gpu_buffer(a, 1, 4)
           || test_packing_gpu_buffer(a, 4, 1)
           || test_packing_gpu_buffer(a, 1, 8)
           || test_packing_gpu_buffer(a, 8, 1)
           || test_packing_gpu_buffer(a, 4, 8)
           || test_packing_gpu_buffer(a, 8, 4)
           || test_packing_gpu_image(a, 1, 1)
           || test_packing_gpu_image(a, 4, 4)
           || test_packing_gpu_image(a, 8, 8)
           || test_packing_gpu_image(a, 1, 4)
           || test_packing_gpu_image(a, 4, 1)
           || test_packing_gpu_image(a, 1, 8)
           || test_packing_gpu_image(a, 8, 1)
           || test_packing_gpu_image(a, 4, 8)
           || test_packing_gpu_image(a, 8, 4)
           || test_packing_gpu_buffer2image(a, 1, 1)
           || test_packing_gpu_buffer2image(a, 4, 4)
           || test_packing_gpu_buffer2image(a, 8, 8)
           || test_packing_gpu_buffer2image(a, 1, 4)
           || test_packing_gpu_buffer2image(a, 4, 1)
           || test_packing_gpu_buffer2image(a, 1, 8)
           || test_packing_gpu_buffer2image(a, 8, 1)
           || test_packing_gpu_buffer2image(a, 4, 8)
           || test_packing_gpu_buffer2image(a, 8, 4)
           || test_packing_gpu_image2buffer(a, 1, 1)
           || test_packing_gpu_image2buffer(a, 4, 4)
           || test_packing_gpu_image2buffer(a, 8, 8)
           || test_packing_gpu_image2buffer(a, 1, 4)
           || test_packing_gpu_image2buffer(a, 4, 1)
           || test_packing_gpu_image2buffer(a, 1, 8)
           || test_packing_gpu_image2buffer(a, 8, 1)
           || test_packing_gpu_image2buffer(a, 4, 8)
           || test_packing_gpu_image2buffer(a, 8, 4)
#endif // NCNN_VULKAN
#if NCNN_CUDA
              || test_packing_gpu(a, 1, 1)
              || test_packing_gpu(a, 4, 4)
              || test_packing_gpu(a, 4, 8)
              || test_packing_gpu(a, 1, 4)
              || test_packing_gpu(a, 4, 1)
              || test_packing_gpu(a, 1, 8)
              || test_packing_gpu(a, 8, 1)
              || test_packing_gpu(a, 4, 8)
              || test_packing_gpu(a, 8, 4)
#endif //NCNN_CUDA
           ;
}

static int test_packing_2()
{
    ncnn::Mat a = RandomMat(40);

    return 0
           || test_packing_cpu(a, 1, 1)
           || test_packing_cpu(a, 4, 4)
           || test_packing_cpu(a, 4, 8)
           || test_packing_cpu(a, 1, 4)
           || test_packing_cpu(a, 4, 1)
           || test_packing_cpu(a, 1, 8)
           || test_packing_cpu(a, 8, 1)
           || test_packing_cpu(a, 4, 8)
           || test_packing_cpu(a, 8, 4)
#if NCNN_VULKAN
           || test_packing_gpu_buffer(a, 1, 1)
           || test_packing_gpu_buffer(a, 4, 4)
           || test_packing_gpu_buffer(a, 8, 8)
           || test_packing_gpu_buffer(a, 1, 4)
           || test_packing_gpu_buffer(a, 4, 1)
           || test_packing_gpu_buffer(a, 1, 8)
           || test_packing_gpu_buffer(a, 8, 1)
           || test_packing_gpu_buffer(a, 4, 8)
           || test_packing_gpu_buffer(a, 8, 4)
           || test_packing_gpu_image(a, 1, 1)
           || test_packing_gpu_image(a, 4, 4)
           || test_packing_gpu_image(a, 8, 8)
           || test_packing_gpu_image(a, 1, 4)
           || test_packing_gpu_image(a, 4, 1)
           || test_packing_gpu_image(a, 1, 8)
           || test_packing_gpu_image(a, 8, 1)
           || test_packing_gpu_image(a, 4, 8)
           || test_packing_gpu_image(a, 8, 4)
           || test_packing_gpu_buffer2image(a, 1, 1)
           || test_packing_gpu_buffer2image(a, 4, 4)
           || test_packing_gpu_buffer2image(a, 8, 8)
           || test_packing_gpu_buffer2image(a, 1, 4)
           || test_packing_gpu_buffer2image(a, 4, 1)
           || test_packing_gpu_buffer2image(a, 1, 8)
           || test_packing_gpu_buffer2image(a, 8, 1)
           || test_packing_gpu_buffer2image(a, 4, 8)
           || test_packing_gpu_buffer2image(a, 8, 4)
           || test_packing_gpu_image2buffer(a, 1, 1)
           || test_packing_gpu_image2buffer(a, 4, 4)
           || test_packing_gpu_image2buffer(a, 8, 8)
           || test_packing_gpu_image2buffer(a, 1, 4)
           || test_packing_gpu_image2buffer(a, 4, 1)
           || test_packing_gpu_image2buffer(a, 1, 8)
           || test_packing_gpu_image2buffer(a, 8, 1)
           || test_packing_gpu_image2buffer(a, 4, 8)
           || test_packing_gpu_image2buffer(a, 8, 4)
#endif // NCNN_VULKAN
#if NCNN_CUDA
          || test_packing_gpu(a, 1, 1)
          || test_packing_gpu(a, 4, 4)
          || test_packing_gpu(a, 4, 8)
          || test_packing_gpu(a, 1, 4)
          || test_packing_gpu(a, 4, 1)
          || test_packing_gpu(a, 1, 8)
          || test_packing_gpu(a, 8, 1)
          || test_packing_gpu(a, 4, 8)
          || test_packing_gpu(a, 8, 4)
#endif //NCNN_CUDA
           ;
}


static int test_packing_gpu()
{
    ncnn::Mat a = RandomMat(1280, 720, 16);
    ncnn::Mat b = RandomMat(1980, 1080, 3);

    return 0
           || test_packing_cpu(a, 1, 1)
           || test_packing_cpu(a, 4, 4)
           || test_packing_cpu(a, 4, 8)
           || test_packing_cpu(a, 1, 4)
           || test_packing_cpu(a, 4, 1)
           || test_packing_cpu(a, 1, 8)
           || test_packing_cpu(a, 8, 1)
           || test_packing_cpu(a, 4, 8)
           || test_packing_cpu(a, 8, 4)
           || test_packing_cpu(b, 1, 1)
           || test_packing_cpu(b, 4, 4)
           || test_packing_cpu(b, 4, 8)
           || test_packing_cpu(b, 1, 4)
           || test_packing_cpu(b, 4, 1)
           || test_packing_cpu(b, 1, 8)
           || test_packing_cpu(b, 8, 1)
           || test_packing_cpu(b, 4, 8)
           || test_packing_cpu(b, 8, 4)
           #if NCNN_VULKAN
           || test_packing_gpu_buffer(a, 1, 1)
           || test_packing_gpu_buffer(a, 4, 4)
           || test_packing_gpu_buffer(a, 8, 8)
           || test_packing_gpu_buffer(a, 1, 4)
           || test_packing_gpu_buffer(a, 4, 1)
           || test_packing_gpu_buffer(a, 1, 8)
           || test_packing_gpu_buffer(a, 8, 1)
           || test_packing_gpu_buffer(a, 4, 8)
           || test_packing_gpu_buffer(a, 8, 4)
           || test_packing_gpu_image(a, 1, 1)
           || test_packing_gpu_image(a, 4, 4)
           || test_packing_gpu_image(a, 8, 8)
           || test_packing_gpu_image(a, 1, 4)
           || test_packing_gpu_image(a, 4, 1)
           || test_packing_gpu_image(a, 1, 8)
           || test_packing_gpu_image(a, 8, 1)
           || test_packing_gpu_image(a, 4, 8)
           || test_packing_gpu_image(a, 8, 4)
           || test_packing_gpu_buffer2image(a, 1, 1)
           || test_packing_gpu_buffer2image(a, 4, 4)
           || test_packing_gpu_buffer2image(a, 8, 8)
           || test_packing_gpu_buffer2image(a, 1, 4)
           || test_packing_gpu_buffer2image(a, 4, 1)
           || test_packing_gpu_buffer2image(a, 1, 8)
           || test_packing_gpu_buffer2image(a, 8, 1)
           || test_packing_gpu_buffer2image(a, 4, 8)
           || test_packing_gpu_buffer2image(a, 8, 4)
           || test_packing_gpu_image2buffer(a, 1, 1)
           || test_packing_gpu_image2buffer(a, 4, 4)
           || test_packing_gpu_image2buffer(a, 8, 8)
           || test_packing_gpu_image2buffer(a, 1, 4)
           || test_packing_gpu_image2buffer(a, 4, 1)
           || test_packing_gpu_image2buffer(a, 1, 8)
           || test_packing_gpu_image2buffer(a, 8, 1)
           || test_packing_gpu_image2buffer(a, 4, 8)
           || test_packing_gpu_image2buffer(a, 8, 4)
           #endif // NCNN_VULKAN
           #if NCNN_CUDA
           || test_packing_gpu(a, 1, 1)
           || test_packing_gpu(a, 4, 4)
           || test_packing_gpu(a, 4, 8)
           || test_packing_gpu(a, 1, 4)
           || test_packing_gpu(a, 4, 1)
           || test_packing_gpu(a, 1, 8)
           || test_packing_gpu(a, 8, 1)
           || test_packing_gpu(a, 4, 8)
           || test_packing_gpu(a, 8, 4)
           || test_packing_gpu(b, 1, 1)
           || test_packing_gpu(b, 4, 4)
           || test_packing_gpu(b, 4, 8)
           || test_packing_gpu(b, 1, 4)
           || test_packing_gpu(b, 4, 1)
           || test_packing_gpu(b, 1, 8)
           || test_packing_gpu(b, 8, 1)
           || test_packing_gpu(b, 4, 8)
           || test_packing_gpu(b, 8, 4)
#endif // NCNN_CUDA
        ;
}

int main()
{
    SRAND(7767517);

    return 0
           || test_packing_0()
           || test_packing_1()
           || test_packing_2()
#if NCNN_CUDA
           || test_packing_gpu()
#endif
        ;
}
