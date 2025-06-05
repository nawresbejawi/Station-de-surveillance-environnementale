/* USER CODE BEGIN Includes */
#include "tx_api.h"
#include "app_filex.h"
#include "DHT11.h"
#include "main.h"
#include <stdio.h>
#include <string.h>
/* USER CODE END Includes */

/* USER CODE BEGIN PV */
/* D�finition des threads et des s�maphores */
TX_THREAD thread_reader;
TX_THREAD thread_writer;
TX_SEMAPHORE data_semaphore;

/* Structure pour stocker les donn�es partag�es */
typedef struct {
    float temperature;
    float humidity;
    char formatted_data[50];
} SharedData;

SharedData shared_data;

/* D�finition de la m�moire des threads */
#define THREAD_STACK_SIZE 1024
uint8_t thread_reader_stack[THREAD_STACK_SIZE];
uint8_t thread_writer_stack[THREAD_STACK_SIZE];

/* D�finition des fichiers FileX */
FX_MEDIA sdio_disk;
FX_FILE fx_file;

uint32_t media_memory[512 / sizeof(uint32_t)];
CHAR filename[] = "nawres_projet_pfa.csv";
/* USER CODE END PV */

/* USER CODE BEGIN PFP */
void thread_reader_entry(ULONG thread_input);
void thread_writer_entry(ULONG thread_input);
/* USER CODE END PFP */

/* USER CODE BEGIN 0 */
/**
 * @brief Initialisation de FileX et cr�ation des threads.
 */
UINT MX_FileX_Init(void) {
    UINT status;

    /* Initialisation de FileX */
    fx_system_initialize();

    /* Ouverture du m�dia SDIO */
    status = fx_media_open(&sdio_disk, "STM32_SDIO_DISK", fx_stm32_sd_driver, 0, (VOID *)media_memory, sizeof(media_memory));
    if (status != FX_SUCCESS) {
        Error_Handler();
    }

    /* Cr�ation du s�maphore pour la synchronisation */
    status = tx_semaphore_create(&data_semaphore, "Data Semaphore", 0);
    if (status != TX_SUCCESS) {
        Error_Handler();
    }

    /* Cr�ation du thread de lecture */
    status = tx_thread_create(&thread_reader, "Thread Reader", thread_reader_entry, 0,
                               thread_reader_stack, THREAD_STACK_SIZE, 5, 5, TX_NO_TIME_SLICE, TX_AUTO_START);
    if (status != TX_SUCCESS) {
        Error_Handler();
    }

    /* Cr�ation du thread d'�criture */
    status = tx_thread_create(&thread_writer, "Thread Writer", thread_writer_entry, 0,
                               thread_writer_stack, THREAD_STACK_SIZE, 5, 5, TX_NO_TIME_SLICE, TX_AUTO_START);
    if (status != TX_SUCCESS) {
        Error_Handler();
    }
    return status;

}

/**
 * @brief Thread de lecture des donn�es.
 * Ce thread lit les donn�es des capteurs et les formate.
 */
void thread_reader_entry(ULONG thread_input) {
    while (1) {
        /* Simulation de lecture des donn�es du capteur */
        DHT11_Data sensor_data = DHT11_ReadData();
        shared_data.temperature = sensor_data.temperature;
        shared_data.humidity = sensor_data.humidity;

        /* Formater les donn�es */
        sprintf(shared_data.formatted_data, "Temperature: %.1f�C, Humidity: %.1f%%\n",
                shared_data.temperature, shared_data.humidity);

        /* Signaler au thread d'�criture que les donn�es sont pr�tes */
        tx_semaphore_put(&data_semaphore);

        /* Attendre avant la prochaine lecture */
        tx_thread_sleep(100); // 100 ticks (ajuster selon la fr�quence souhait�e)
    }
}

/**
 * @brief Thread d'�criture des donn�es dans un fichier.
 * Ce thread �crit les donn�es format�es dans un fichier Excel.
 */
void thread_writer_entry(ULONG thread_input) {
    UINT status;

    /* Cr�er le fichier si n�cessaire */
    status = fx_file_create(&sdio_disk, filename);
    if (status != FX_SUCCESS && status != FX_ALREADY_CREATED) {
        Error_Handler();
    }

    /* Ouvrir le fichier en mode �criture */
    status = fx_file_open(&sdio_disk, &fx_file, filename, FX_OPEN_FOR_WRITE);
    if (status != FX_SUCCESS) {
        Error_Handler();
    }

    /* Positionner le curseur � la fin du fichier */
    fx_file_seek(&fx_file, 0);

    while (1) {
        /* Attendre que les donn�es soient pr�tes */
        tx_semaphore_get(&data_semaphore, TX_WAIT_FOREVER);

        /* �crire les donn�es dans le fichier */
        status = fx_file_write(&fx_file, shared_data.formatted_data, strlen(shared_data.formatted_data));
        if (status != FX_SUCCESS) {
            Error_Handler();
        }

        /* Forcer la sauvegarde des donn�es sur le disque */
        fx_media_flush(&sdio_disk);
    }

    /* Fermer le fichier (non atteint dans ce cas) */
    fx_file_close(&fx_file);
}
DHT11_Data DHT11_ReadData(void)
{
    uint8_t data[5] = {0}; // Array to store data
    uint8_t i, j;
    DHT11_Data result = {0, 0}; // Initialize the result structure with default values (0, 0)

    // Step 1: Send start signal
    Set_Pin_Output(DHT11_PORT, DHT11_PIN); // Set the pin as output
    HAL_GPIO_WritePin(DHT11_PORT, DHT11_PIN, GPIO_PIN_RESET); // Pull the pin low
    HAL_Delay(18); // Wait for 18 ms
    HAL_GPIO_WritePin(DHT11_PORT, DHT11_PIN, GPIO_PIN_SET); // Pull the pin high
    delay(20); // Wait for 20 �s
    Set_Pin_Input(DHT11_PORT, DHT11_PIN); // Set the pin as input

    // Step 2: Wait for response from DHT11
    if (!HAL_GPIO_ReadPin(DHT11_PORT, DHT11_PIN))
    {
        delay(80); // Wait for 80 �s
        if (HAL_GPIO_ReadPin(DHT11_PORT, DHT11_PIN))
        {
            delay(80); // Wait another 80 �s
        }
        else
        {
            // Error: No response
            return result; // Return default values (0, 0)
        }
    }

    // Step 3: Read 40 bits of data
    for (i = 0; i < 5; i++)
    {
        for (j = 0; j < 8; j++)
        {
            // Wait for the start of the bit
            while (!HAL_GPIO_ReadPin(DHT11_PORT, DHT11_PIN));

            // Measure the duration of the high signal
            delay(40); // Wait 40 �s
            if (HAL_GPIO_ReadPin(DHT11_PORT, DHT11_PIN))
            {
                data[i] |= (1 << (7 - j)); // Store a "1"
                while (HAL_GPIO_ReadPin(DHT11_PORT, DHT11_PIN)); // Wait for the end of the bit
            }
        }
    }

    // Step 4: Verify checksum
    if (data[4] == (data[0] + data[1] + data[2] + data[3]))
    {
        result.humidity = data[0] + data[1] * 0.1;
        result.temperature = data[2] + (data[3] & 0x7F) * 0.1;
        if (data[3] & 0x80) result.temperature *= -1; // Negative temperature
    }
    else
    {
        // Checksum error
        result.humidity = 0;
        result.temperature = 0;
    }

    return result;
}

/* USER CODE END 0 */