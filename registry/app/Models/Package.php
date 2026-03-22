<?php

namespace App\Models;

use Illuminate\Database\Eloquent\Model;

class Package extends Model
{
    protected $fillable = ['name', 'description'];

    public function versions()
    {
        return $this->hasMany(PackageVersion::class);
    }
}
