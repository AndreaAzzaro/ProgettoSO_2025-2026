#ifndef STATS_H
#define STATS_H

typedef struct{
    int first_course;
    int second_course;
    int coffe_dessert;
    int total_served;
}Food;

typedef struct{
    double primi;
    double secondi;
    double coffee;
    double cassa;
    double global_avg;
}Time;


#endif