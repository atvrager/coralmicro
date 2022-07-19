#include <PDM.h>
#include <coralmicro_SD.h>

#include "Arduino.h"
#include "coral_micro.h"
#include "libs/tensorflow/classification.h"
#include "libs/tensorflow/yamnet.h"
#include "third_party/tflite-micro/tensorflow/lite/experimental/microfrontend/lib/frontend.h"

namespace {
std::vector<int32_t> currentSamples;
tflite::MicroMutableOpResolver</*tOpsCount=*/3> resolver;
const tflite::Model* model = nullptr;
std::vector<uint8_t> model_data;
std::shared_ptr<coralmicro::EdgeTpuContext> context = nullptr;
std::unique_ptr<tflite::MicroInterpreter> interpreter = nullptr;
TfLiteTensor* input_tensor = nullptr;

constexpr int kTensorArenaSize = 1 * 1024 * 1024;
STATIC_TENSOR_ARENA_IN_SDRAM(tensor_arena, kTensorArenaSize);

FrontendState frontend_state{};
constexpr float kThreshold = 0.3;
constexpr int kTopK = 5;

constexpr char kModelName[] = "/models/yamnet_edgetpu.tflite";
}  // namespace

void setup() {
    Serial.begin(115200);
    SD.begin();
    Mic.begin();

    tflite::MicroErrorReporter error_reporter;
    resolver = coralmicro::tensorflow::SetupYamNetResolver</*tForTpu=*/true>();

    Serial.println("Loading Model");

    if (!SD.exists(kModelName)) {
        Serial.println("Model file not found");
        return;
    }

    SDFile model_file = SD.open(kModelName);
    uint32_t model_size = model_file.size();
    model_data.resize(model_size);
    if (model_file.read(model_data.data(), model_size) != model_size) {
        Serial.print("Error loading model");
    }

    model = tflite::GetModel(model_data.data());
    context = coralmicro::EdgeTpuManager::GetSingleton()->OpenDevice();
    if (!context) {
        Serial.println("Failed to get EdgeTpuContext");
        return;
    }

    interpreter = std::make_unique<tflite::MicroInterpreter>(
        model, resolver, tensor_arena, kTensorArenaSize, &error_reporter);
    if (interpreter->AllocateTensors() != kTfLiteOk) {
        printf("AllocateTensors failed.\r\n");
        vTaskSuspend(nullptr);
    }

    if (!coralmicro::tensorflow::YamNetPrepareFrontEnd(&frontend_state)) {
        Serial.println(
            "coralmicro::tensorflow::YamNetPrepareFrontEnd() failed.");
        return;
    }

    Serial.println("YAMNet Setup Complete\r\n\n");
}

void loop() {
    if (!interpreter) {
        Serial.println("Cannot invoke because of a problem during setup!");
        return;
    }

    input_tensor = interpreter->input_tensor(0);
    auto audio_input = tflite::GetTensorData<int16_t>(input_tensor);

    int samplesRead = Mic.available();
    Mic.read(currentSamples, samplesRead);

    for (int i = 0; i < currentSamples.size(); ++i) {
        audio_input[i] = currentSamples[i] >> 16;
    }

    coralmicro::tensorflow::YamNetPreprocessInput(input_tensor,
                                                  &frontend_state);
    // Reset frontend state.
    FrontendReset(&frontend_state);
    if (interpreter->Invoke() != kTfLiteOk) {
        printf("Failed to invoke on test input\r\n");
        vTaskSuspend(nullptr);
    }

    auto results = coralmicro::tensorflow::GetClassificationResults(
        interpreter.get(), kThreshold, kTopK);
    if (results.empty()) {
        Serial.println("No results");
    } else {
        Serial.println("Results");
        for (const auto& c : results) {
            Serial.print(c.id);
            Serial.print(": ");
            Serial.println(c.score);
        }
    }

    delay(coralmicro::tensorflow::kYamnetDurationMs);
}