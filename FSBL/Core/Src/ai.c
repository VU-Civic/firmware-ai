// Header Inclusions ---------------------------------------------------------------------------------------------------

#include "ai.h"
#include "ll_aton_runtime.h"


// Static AI Network Variables -----------------------------------------------------------------------------------------

LL_ATON_DECLARE_NAMED_NN_INSTANCE_AND_INTERFACE(Default)
static uint8_t *data_in, *data_out, *data_in_end, *data_out_end;


// Public API Functions ------------------------------------------------------------------------------------------------

void ai_init(void)
{
   // Retrieve pointers to the AI input and output buffers
   const LL_Buffer_InfoTypeDef* input_buffers = NN_Interface_Default.input_buffers_info();
   const LL_Buffer_InfoTypeDef* output_buffers = NN_Interface_Default.output_buffers_info();
   data_in = (uint8_t*)LL_Buffer_addr_start(&input_buffers[0]);
   data_in_end = (uint8_t*)LL_Buffer_addr_end(&input_buffers[0]);
   data_out = (uint8_t*)LL_Buffer_addr_start(&output_buffers[0]);
   data_out_end = (uint8_t*)LL_Buffer_addr_end(&output_buffers[0]);
   // TODO: DO WE NEED THIS
   float input_inv_scale = input_buffers[0].scale[0] ? (1.0f / input_buffers[0].scale[0]) : 1.0f;
   int16_t input_offset = input_buffers[0].offset[0];

   // Invalidate all caches and clear the NPU pipeline
   npu_cache_invalidate();
   mcu_cache_clean_invalidate();
   uint32_t t = ATON_CLKCTRL_CTRL_GET(0);
   t = ATON_CLKCTRL_CTRL_SET_CLR(t, 1);
   ATON_CLKCTRL_CTRL_SET(0, t);

   // Initialize the AI runtime
   LL_ATON_RT_RuntimeInit();
   LL_ATON_RT_Init_Network(&NN_Instance_Default);
}

void ai_process(void)
{
   // Invalidate all caches and reset the AI runtime
   LL_ATON_Cache_NPU_Invalidate();
   LL_ATON_Cache_MCU_Clean_Invalidate_Range((uintptr_t)data_in, data_in_end - data_in);
   LL_ATON_Cache_MCU_Invalidate_Range((uintptr_t)data_out, data_out_end - data_out);
   LL_ATON_RT_Reset_Network(&NN_Instance_Default);

   // Run the inference loop
   LL_ATON_RT_RetValues_t ll_aton_rt_ret = LL_ATON_RT_DONE;
   do
   {
      ll_aton_rt_ret = LL_ATON_RT_RunEpochBlock(&NN_Instance_Default);
      if (ll_aton_rt_ret == LL_ATON_RT_WFE)
         LL_ATON_OSAL_WFE();
   } while (ll_aton_rt_ret != LL_ATON_RT_DONE);

   // Process the classification output
   /* Invalidate the associated CPU cache region if requested */
   //_post_process(buffer_out);
}
